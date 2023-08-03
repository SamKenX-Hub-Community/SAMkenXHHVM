(*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

val get_current_decl : Typing_env_types.env -> Decl_reference.t option

val make_depend_on_gconst :
  Typing_env_types.env -> string -> Typing_defs.const_decl option -> unit

val make_depend_on_fun :
  Typing_env_types.env -> string -> Typing_defs.fun_elt option -> unit

val make_depend_on_class :
  Typing_env_types.env -> string -> Decl_provider.Class.t option -> unit

val make_depend_on_module :
  Typing_env_types.env -> string -> Typing_defs.module_def_type option -> unit

val make_depend_on_module_name : Typing_env_types.env -> string -> unit

val make_depend_on_typedef :
  Typing_env_types.env -> string -> Typing_defs.typedef_type option -> unit

val make_depend_on_constructor :
  Typing_env_types.env -> Decl_provider.Class.t -> unit

val make_depend_on_class_const :
  Typing_env_types.env -> Decl_provider.Class.t -> string -> unit

val add_member_dep :
  is_method:bool ->
  is_static:bool ->
  Typing_env_types.env ->
  Decl_provider.Class.t ->
  string ->
  Typing_defs.class_elt option ->
  unit

(** Register the droot as being dependent on all of the ancestor classes,
    interfaces, and traits of the given class (i.e., the recursive ancestors
    returned by [Typing_classes_heap.Api.all_ancestor_names] and stored in
    [Decl_defs.dc_ancestors]). Should be invoked once when typechecking the
    given class (after [droot] has been set to correspond to the given class). *)
val make_depend_on_ancestors :
  Typing_env_types.env -> Decl_provider.Class.t -> unit

val make_depend_on_parent :
  Typing_env_types.env ->
  skip_constructor_dep:bool ->
  string ->
  Decl_provider.Class.t option ->
  unit
