use std::sync::Arc;

use ir::instr;
use ir::Attr;
use ir::Attribute;
use ir::Class;
use ir::ClassId;
use ir::Coeffects;
use ir::Constant;
use ir::FuncBuilder;
use ir::HackConstant;
use ir::Instr;
use ir::LocalId;
use ir::MemberOpBuilder;
use ir::Method;
use ir::MethodFlags;
use ir::MethodId;
use ir::Param;
use ir::PropId;
use ir::Property;
use ir::StringInterner;
use ir::TypeInfo;
use ir::Visibility;
use log::trace;

use crate::class::IsStatic;

/// This indicates a static property that started life as a class constant.
pub(crate) const INFER_CONSTANT: &str = "__Infer_Constant__";

pub(crate) fn lower_class<'a>(mut class: Class<'a>, strings: Arc<StringInterner>) -> Class<'a> {
    if !class.ctx_constants.is_empty() {
        textual_todo! {
            trace!("TODO: class.ctx_constants");
        }
    }

    if !class.upper_bounds.is_empty() {
        textual_todo! {
            trace!("TODO: class.upper_bounds");
        }
    }

    for method in &mut class.methods {
        if method.name.is_86pinit(&strings) {
            // We want 86pinit to be 'instance' but hackc marks it as 'static'.
            method.attrs -= Attr::AttrStatic;
        }
    }

    // HHVM is okay with implicit 86pinit and 86sinit but we need to make them
    // explicit so we can put trivial initializers in them (done later in func
    // lowering).
    create_method_if_missing(
        &mut class,
        MethodId::_86pinit(&strings),
        IsStatic::NonStatic,
        Arc::clone(&strings),
    );
    create_method_if_missing(
        &mut class,
        MethodId::_86sinit(&strings),
        IsStatic::Static,
        Arc::clone(&strings),
    );

    if class.flags.contains(Attr::AttrIsClosureClass) {
        create_default_closure_constructor(&mut class, Arc::clone(&strings));
    }

    // Turn class constants into properties.
    // TODO: Need to think about abstract constants. Maybe constants lookups
    // should really be function calls...
    for constant in class.constants.drain(..) {
        let HackConstant {
            name,
            value,
            is_abstract: _,
        } = constant;
        // Mark the property as originally being a constant.
        let attributes = vec![Attribute {
            name: ClassId::from_str(INFER_CONSTANT, &strings),
            arguments: Vec::new(),
        }];
        let type_info = if let Some(value) = value.as_ref() {
            value.type_info()
        } else {
            TypeInfo::empty()
        };
        let prop = Property {
            name: PropId::new(name.id),
            flags: Attr::AttrStatic,
            attributes,
            visibility: Visibility::Public,
            initial_value: value,
            type_info,
            doc_comment: Default::default(),
        };
        class.properties.push(prop);
    }

    class
}

fn create_default_closure_constructor<'a>(class: &mut Class<'a>, strings: Arc<StringInterner>) {
    let name = MethodId::constructor(&strings);

    let func = FuncBuilder::build_func(Arc::clone(&strings), |fb| {
        let loc = fb.add_loc(class.src_loc.clone());
        fb.func.loc_id = loc;

        // '$this' parameter is implied.

        for prop in &class.properties {
            fb.func.params.push(Param {
                name: prop.name.id,
                is_variadic: false,
                is_inout: false,
                is_readonly: false,
                user_attributes: Vec::new(),
                ty: prop.type_info.clone(),
                default_value: None,
            });

            let lid = LocalId::Named(prop.name.id);
            let value = fb.emit(Instr::Hhbc(instr::Hhbc::CGetL(lid, loc)));
            MemberOpBuilder::base_h(loc).emit_set_m_pt(fb, prop.name, value);
        }

        let null = fb.emit_constant(Constant::Null);
        fb.emit(Instr::ret(null, loc));
    });

    let method = Method {
        attributes: Vec::new(),
        attrs: Attr::AttrNone,
        coeffects: Coeffects::default(),
        flags: MethodFlags::empty(),
        func,
        name,
        visibility: Visibility::Public,
    };
    class.methods.push(method);
}

fn create_method_if_missing<'a>(
    class: &mut Class<'a>,
    name: MethodId,
    is_static: IsStatic,
    strings: Arc<StringInterner>,
) {
    if class.methods.iter().any(|m| m.name == name) {
        return;
    }

    let func = FuncBuilder::build_func(strings, |fb| {
        let loc = fb.add_loc(class.src_loc.clone());
        fb.func.loc_id = loc;
        let null = fb.emit_constant(Constant::Null);
        fb.emit(Instr::ret(null, loc));
    });

    let attrs = is_static.as_attr();

    let method = Method {
        attributes: Vec::new(),
        attrs,
        coeffects: Coeffects::default(),
        flags: MethodFlags::empty(),
        func,
        name,
        visibility: Visibility::Private,
    };
    class.methods.push(method);
}
