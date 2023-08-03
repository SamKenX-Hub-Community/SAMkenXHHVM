// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.

use std::cmp::Ordering;
use std::fmt;

use ir::StringInterner;
use naming_special_names_rust::members;

use crate::class::IsStatic;

pub(crate) const TOP_LEVELS_CLASS: &str = "$root";

/// Used for things that can mangle themselves directly.
pub(crate) trait Mangle {
    fn mangle(&self, strings: &StringInterner) -> String;
}

impl Mangle for [u8] {
    fn mangle(&self, _strings: &StringInterner) -> String {
        // Handle some reserved tokens.
        match self {
            b"declare" => "declare_".to_owned(),
            b"define" => "define_".to_owned(),
            b"extends" => "extends_".to_owned(),
            b"false" => "false_".to_owned(),
            b"float" => "float_".to_owned(),
            b"global" => "global_".to_owned(),
            b"int" => "int_".to_owned(),
            b"jmp" => "jmp_".to_owned(),
            b"load" => "load_".to_owned(),
            b"local" => "local_".to_owned(),
            b"null" => "null_".to_owned(),
            b"prune" => "prune_".to_owned(),
            b"ret" => "ret_".to_owned(),
            b"store" => "store_".to_owned(),
            b"throw" => "throw_".to_owned(),
            b"true" => "true_".to_owned(),
            b"type" => "type_".to_owned(),
            b"unreachable" => "unreachable_".to_owned(),
            b"void" => "void_".to_owned(),
            _ => {
                // This mangling is terrible... but probably "good enough".
                // If a digit is first then we prepend a '_'.
                // [A-Za-z0-9_$] -> identity
                // \ -> ::
                // anything else -> xx (hex digits)
                let mut res = String::with_capacity(self.len());
                if self.first().map_or(false, u8::is_ascii_digit) {
                    res.push('_');
                }
                for &ch in self {
                    match ch {
                        b'_' | b'$' => res.push(ch as char),
                        b'\\' => {
                            res.push(':');
                            res.push(':');
                        }
                        ch if ch.is_ascii_alphanumeric() => res.push(ch as char),
                        _ => {
                            res.push(b"0123456789abcdef"[(ch >> 4) as usize] as char);
                            res.push(b"0123456789abcdef"[(ch & 15) as usize] as char);
                        }
                    }
                }
                res
            }
        }
    }
}

// Classes and functions live in different namespaces.

impl Mangle for ir::PropId {
    fn mangle(&self, strings: &StringInterner) -> String {
        self.as_bytes(strings).mangle(strings)
    }
}

#[derive(Eq, PartialEq, Hash, Clone, Debug)]
pub(crate) enum Intrinsic {
    ConstInit(ir::ClassId),
    Construct(ir::ClassId),
    Factory(ir::ClassId),
    Invoke(TypeName),
    PropInit(ir::ClassId),
    StaticInit(ir::ClassId),
}

impl Intrinsic {
    pub(crate) fn contains_unknown(&self) -> bool {
        match self {
            Intrinsic::ConstInit(_)
            | Intrinsic::Construct(_)
            | Intrinsic::Factory(_)
            | Intrinsic::PropInit(_)
            | Intrinsic::StaticInit(_) => false,
            Intrinsic::Invoke(name) => *name == TypeName::Unknown,
        }
    }
}

/// Represents a named callable thing.  This includes top-level functions and
/// methods.
#[derive(Eq, PartialEq, Hash, Clone, Debug)]
pub(crate) enum FunctionName {
    Builtin(crate::hack::Builtin),
    Function(ir::FunctionId),
    Intrinsic(Intrinsic),
    Method(TypeName, ir::MethodId),
    Unmangled(String),
}

impl FunctionName {
    pub(crate) fn method(class: ir::ClassId, is_static: IsStatic, method: ir::MethodId) -> Self {
        Self::Method(TypeName::class(class, is_static), method)
    }

    pub(crate) fn untyped_method(method: ir::MethodId) -> Self {
        Self::Method(TypeName::Unknown, method)
    }

    pub(crate) fn display<'r>(&'r self, strings: &'r StringInterner) -> impl fmt::Display + 'r {
        FmtFunctionName(strings, self)
    }

    pub(crate) fn cmp(&self, other: &Self, strings: &StringInterner) -> Ordering {
        let a = self.display(strings).to_string();
        let b = other.display(strings).to_string();
        a.cmp(&b)
    }

    pub(crate) fn contains_unknown(&self) -> bool {
        match self {
            FunctionName::Builtin(_) | FunctionName::Function(_) | FunctionName::Unmangled(_) => {
                false
            }
            FunctionName::Intrinsic(intrinsic) => intrinsic.contains_unknown(),
            FunctionName::Method(name, _) => *name == TypeName::Unknown,
        }
    }
}

struct FmtFunctionName<'a>(&'a StringInterner, &'a FunctionName);

impl fmt::Display for FmtFunctionName<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let FmtFunctionName(strings, name) = *self;
        match name {
            FunctionName::Builtin(b) => b.as_str().fmt(f)?,
            FunctionName::Function(fid) => write!(
                f,
                "{TOP_LEVELS_CLASS}.{}",
                fid.as_bytes(strings).mangle(strings)
            )?,
            FunctionName::Intrinsic(intrinsic) => {
                let tn;
                let (ty, name) = match intrinsic {
                    Intrinsic::ConstInit(cid) => {
                        tn = TypeName::StaticClass(*cid);
                        (Some(&tn), "_86cinit")
                    }
                    Intrinsic::Construct(cid) => {
                        tn = TypeName::Class(*cid);
                        (Some(&tn), members::__CONSTRUCT)
                    }
                    Intrinsic::Factory(cid) => {
                        tn = TypeName::StaticClass(*cid);
                        (Some(&tn), "__factory")
                    }
                    Intrinsic::Invoke(tn) => (Some(tn), "__invoke"),
                    Intrinsic::PropInit(cid) => {
                        tn = TypeName::Class(*cid);
                        (Some(&tn), "_86pinit")
                    }
                    Intrinsic::StaticInit(cid) => {
                        tn = TypeName::StaticClass(*cid);
                        (Some(&tn), "_86sinit")
                    }
                };
                if let Some(ty) = ty {
                    write!(f, "{}.{}", ty.display(strings), name)?;
                } else {
                    f.write_str(name)?;
                }
            }
            FunctionName::Method(class, mid) => {
                write!(
                    f,
                    "{}.{}",
                    class.display(strings),
                    mid.as_bytes(strings).mangle(strings)
                )?;
            }
            FunctionName::Unmangled(s) => s.fmt(f)?,
        }
        Ok(())
    }
}

#[derive(Eq, PartialEq, Hash, Clone, Debug)]
pub(crate) enum GlobalName {
    Global(ir::GlobalId),
    GlobalConst(ir::GlobalId),
}

impl GlobalName {
    pub(crate) fn display<'r>(&'r self, strings: &'r StringInterner) -> impl fmt::Display + 'r {
        FmtGlobalName(strings, self)
    }

    pub(crate) fn cmp(&self, other: &Self, strings: &StringInterner) -> Ordering {
        let a = self.display(strings).to_string();
        let b = other.display(strings).to_string();
        a.cmp(&b)
    }
}

struct FmtGlobalName<'a>(&'a StringInterner, &'a GlobalName);

impl fmt::Display for FmtGlobalName<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let FmtGlobalName(strings, name) = *self;
        match name {
            GlobalName::Global(id) => {
                write!(f, "global::{}", id.as_bytes(strings).mangle(strings))
            }
            GlobalName::GlobalConst(id) => {
                write!(f, "gconst::{}", id.as_bytes(strings).mangle(strings))
            }
        }
    }
}

#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub(crate) enum TypeName {
    Class(ir::ClassId),
    Curry(Box<FunctionName>),
    StaticClass(ir::ClassId),
    Unknown,
    UnmangledRef(&'static str),
}

impl TypeName {
    pub(crate) fn class(class: ir::ClassId, is_static: crate::class::IsStatic) -> Self {
        match is_static {
            IsStatic::Static => Self::StaticClass(class),
            IsStatic::NonStatic => Self::Class(class),
        }
    }

    pub(crate) fn display<'r>(&'r self, strings: &'r StringInterner) -> impl fmt::Display + 'r {
        FmtTypeName(strings, self)
    }
}

struct FmtTypeName<'a>(&'a StringInterner, &'a TypeName);

impl fmt::Display for FmtTypeName<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let FmtTypeName(strings, name) = *self;
        match name {
            TypeName::Class(cid) => f.write_str(&cid.as_bytes(strings).mangle(strings)),
            TypeName::Curry(box FunctionName::Function(fid)) => {
                write!(f, "{}$curry", fid.as_bytes(strings).mangle(strings))
            }
            TypeName::Curry(box FunctionName::Method(ty, mid)) => {
                write!(
                    f,
                    "{}_{}$curry",
                    ty.display(strings),
                    mid.as_bytes(strings).mangle(strings)
                )
            }
            TypeName::Curry(_) => panic!("Unable to name curry type {name:?}"),
            TypeName::Unknown => f.write_str("?"),
            TypeName::StaticClass(cid) => {
                f.write_str(&cid.as_bytes(strings).mangle(strings))?;
                f.write_str("$static")
            }
            TypeName::UnmangledRef(s) => s.fmt(f),
        }
    }
}
