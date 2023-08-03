// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.

use nast::Contexts;
use nast::Expr_;
use nast::Hint;
use nast::Hint_;

use crate::prelude::*;

#[derive(Clone, Copy, Default)]
pub struct ElabHintWildcardPass {
    depth: usize,
    allow_wildcard: bool,
}

impl ElabHintWildcardPass {
    pub fn reset_depth(&mut self) {
        self.depth = 0
    }
    pub fn incr_depth(&mut self) {
        self.depth += 1
    }
}

impl Pass for ElabHintWildcardPass {
    fn on_ty_hint_top_down(&mut self, env: &Env, elem: &mut Hint) -> ControlFlow<()> {
        let Hint(pos, box hint_) = elem;
        //   Swap for `Herr`
        let in_hint_ = std::mem::replace(hint_, Hint_::Herr);
        match &in_hint_ {
            Hint_::Hwildcard => {
                if self.allow_wildcard && self.depth >= 1 {
                    // This is valid; restore the hint and continue
                    *hint_ = in_hint_;
                    Continue(())
                } else {
                    // Wildcard hints are disallowed here; add an error
                    env.emit_error(NamingError::WildcardHintDisallowed(pos.clone()));
                    //  We've already set the hint to `Herr` so just break
                    Break(())
                }
            }
            _ => {
                // This isn't a wildcard hint; restore the original hint and continue
                *hint_ = in_hint_;
                Continue(())
            }
        }
    }

    // Wildcard hints are _always_ disallowed in contexts
    // TODO: we define this on `context` in OCaml - we need a newtype
    // to do the same here
    fn on_ty_contexts_top_down(&mut self, env: &Env, elem: &mut Contexts) -> ControlFlow<()> {
        let Contexts(_, hints) = elem;
        hints
            .iter_mut()
            .filter(|hint| is_wildcard(hint))
            .for_each(|hint| {
                let Hint(pos, box hint_) = hint;
                env.emit_error(NamingError::InvalidWildcardContext(pos.clone()));
                *hint_ = Hint_::Herr
            });
        Continue(())
    }

    fn on_ty_expr__top_down(&mut self, _: &Env, elem: &mut nast::Expr_) -> ControlFlow<()> {
        match elem {
            Expr_::Cast(..) => self.incr_depth(),
            Expr_::Is(..) | Expr_::As(..) => self.allow_wildcard = true,
            Expr_::Upcast(..) => self.allow_wildcard = false,
            _ => (),
        }
        Continue(())
    }

    fn on_ty_targ_top_down(&mut self, _: &Env, _: &mut nast::Targ) -> ControlFlow<()> {
        self.allow_wildcard = true;
        self.incr_depth();
        Continue(())
    }

    fn on_ty_hint__top_down(&mut self, _: &Env, elem: &mut Hint_) -> ControlFlow<()> {
        match elem {
            Hint_::Hunion(_)
            | Hint_::Hintersection(_)
            | Hint_::Hoption(_)
            | Hint_::Hlike(_)
            | Hint_::Hsoft(_)
            | Hint_::Hrefinement(..) => self.reset_depth(),
            Hint_::Htuple(_) | Hint_::Happly(..) | Hint_::Habstr(..) | Hint_::HvecOrDict(..) => {
                self.incr_depth()
            }
            _ => (),
        }
        Continue(())
    }

    fn on_ty_shape_field_info_top_down(
        &mut self,
        _: &Env,
        _: &mut nast::ShapeFieldInfo,
    ) -> ControlFlow<()> {
        self.incr_depth();
        Continue(())
    }
}

fn is_wildcard(hint: &Hint) -> bool {
    match hint {
        Hint(_, box Hint_::Hwildcard) => true,
        _ => false,
    }
}

#[cfg(test)]
mod tests {

    use nast::Pos;
    use nast::Targ;

    use super::*;

    fn make_wildcard() -> Hint {
        Hint(Pos::default(), Box::new(Hint_::Hwildcard))
    }

    // -- Wildcard hints in expressions ----------------------------------------

    // -- Wilcard hints in `Is` (equivalently `As`) expressions ----------------

    #[test]
    // Wildcard hint at the top-level of a `Is` expression. `allow_wildcard`
    // will be true because of the `Is` expression but depth will be 0 since
    // we reach the `on_ty_hint_top_down` handler _before_ `on_ty_hint__top_down`
    // As a result, we will mark the hint as invalid and raise an error
    fn test_expr_is_top_level_invalid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Expr_::Is(Box::new((elab_utils::expr::null(), make_wildcard())));
        elem.transform(&env, &mut pass);

        let wildcard_hint_disallowed_err_opt = env.into_errors().pop();
        assert!(matches!(
            wildcard_hint_disallowed_err_opt,
            Some(NamingPhaseError::Naming(
                NamingError::WildcardHintDisallowed(_)
            ))
        ));

        assert!(matches!(elem, Expr_::Is(box (_, Hint(_, box Hint_::Herr)))))
    }

    #[test]
    // Wildcard hint inside a tuple hint, in a `Is` expression. This means
    // we have (i) wildcards are allowed, (ii) depth is 1, (iii) the
    // wildcard isn't applied to other types so we expect it to be valid
    fn test_expr_is_nested_valid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Expr_::Is(Box::new((
            elab_utils::expr::null(),
            Hint(
                Pos::default(),
                Box::new(Hint_::Htuple(vec![make_wildcard()])),
            ),
        )));
        elem.transform(&env, &mut pass);

        assert!(env.into_errors().is_empty());
        assert!(match elem {
            Expr_::Is(box (_, Hint(_, box Hint_::Htuple(mut hints)))) =>
                hints.pop().map_or(false, |hint| is_wildcard(&hint)),
            _ => false,
        })
    }

    // -- Wildcard hint in `Upcast` expressions --------------------------------
    #[test]
    // Since `Upcast` sets `allow_wildcard` to false and the flag is only
    // set  to true in `targ`s or `Is`/`As` expressions we expect all wildcard
    // hints appearing here to be invalid
    fn test_expr_upcast_top_level_invalid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Expr_::Upcast(Box::new((elab_utils::expr::null(), make_wildcard())));
        elem.transform(&env, &mut pass);

        let wildcard_hint_disallowed_err_opt = env.into_errors().pop();
        assert!(matches!(
            wildcard_hint_disallowed_err_opt,
            Some(NamingPhaseError::Naming(
                NamingError::WildcardHintDisallowed(_)
            ))
        ));

        assert!(matches!(
            elem,
            Expr_::Upcast(box (_, Hint(_, box Hint_::Herr)))
        ))
    }

    // -- Wildcard hint in `Cast` expressions ----------------------------------

    #[test]
    // Since `Cast` does not modify `allow_wildcard` we never expect it to be
    // valid
    fn test_expr_cast_top_level_invalid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Expr_::Cast(Box::new((make_wildcard(), elab_utils::expr::null())));
        elem.transform(&env, &mut pass);

        let wildcard_hint_disallowed_err_opt = env.into_errors().pop();
        assert!(matches!(
            wildcard_hint_disallowed_err_opt,
            Some(NamingPhaseError::Naming(
                NamingError::WildcardHintDisallowed(_)
            ))
        ));

        assert!(matches!(
            elem,
            Expr_::Cast(box (Hint(_, box Hint_::Herr), _))
        ))
    }

    // -- Wildcard hints in `Contexts` -----------------------------------------

    #[test]
    // Wildcard hints are always invalid inside `Contexts`
    fn test_contexts_top_level_invalid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Contexts(Pos::default(), vec![make_wildcard()]);
        elem.transform(&env, &mut pass);

        let invalid_wildcard_context_err_opt = env.into_errors().pop();
        assert!(matches!(
            invalid_wildcard_context_err_opt,
            Some(NamingPhaseError::Naming(
                NamingError::InvalidWildcardContext(_)
            ))
        ));

        let Contexts(_, mut hints) = elem;
        let hint_opt = hints.pop();

        assert!(matches!(hint_opt, Some(Hint(_, box Hint_::Herr))))
    }

    // -- Wildcard hints in `Targ`s --------------------------------------------

    #[test]
    // Wildcard hints are valid inside `Targ`s
    fn test_contexts_top_level_valid() {
        let env = Env::default();

        let mut pass = ElabHintWildcardPass::default();
        let mut elem = Targ((), make_wildcard());
        elem.transform(&env, &mut pass);

        assert!(env.into_errors().is_empty());
        let Targ(_, ref hint) = elem;
        assert!(is_wildcard(hint))
    }
}
