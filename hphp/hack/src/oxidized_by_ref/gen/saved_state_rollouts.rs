// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.
//
// @generated SignedSource<<3b5e5d7c7962e66bdae7d01579254161>>
//
// To regenerate this file, run:
//   hphp/hack/src/oxidized_regen.sh

use arena_trait::TrivialDrop;
use eq_modulo_pos::EqModuloPos;
use no_pos_hash::NoPosHash;
use ocamlrep::FromOcamlRep;
use ocamlrep::FromOcamlRepIn;
use ocamlrep::ToOcamlRep;
use serde::Deserialize;
use serde::Serialize;

#[allow(unused_imports)]
use crate::*;

#[derive(
    Clone,
    Debug,
    Deserialize,
    Eq,
    EqModuloPos,
    FromOcamlRepIn,
    Hash,
    NoPosHash,
    Ord,
    PartialEq,
    PartialOrd,
    Serialize,
    ToOcamlRep
)]
#[rust_to_ocaml(attr = "deriving (eq, show)")]
#[repr(C)]
pub struct SavedStateRollouts {
    pub dummy_one: bool,
    pub dummy_two: bool,
    pub dummy_three: bool,
    /// Whether the depgraph contains the transitive closure of extends edges.
    pub no_ancestor_edges: bool,
}
impl TrivialDrop for SavedStateRollouts {}
arena_deserializer::impl_deserialize_in_arena!(SavedStateRollouts);

#[derive(
    Clone,
    Copy,
    Debug,
    Deserialize,
    Eq,
    EqModuloPos,
    FromOcamlRep,
    FromOcamlRepIn,
    Hash,
    NoPosHash,
    Ord,
    PartialEq,
    PartialOrd,
    Serialize,
    ToOcamlRep
)]
#[rust_to_ocaml(attr = "deriving show { with_path = false }")]
#[repr(u8)]
pub enum Flag {
    #[rust_to_ocaml(name = "Dummy_one")]
    DummyOne,
    #[rust_to_ocaml(name = "Dummy_two")]
    DummyTwo,
    #[rust_to_ocaml(name = "Dummy_three")]
    DummyThree,
    #[rust_to_ocaml(name = "No_ancestor_edges")]
    NoAncestorEdges,
}
impl TrivialDrop for Flag {}
arena_deserializer::impl_deserialize_in_arena!(Flag);

pub type FlagName = str;
