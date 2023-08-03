(**
 * Copyright (c) 2019, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

open Hh_prelude
open ClassDiff
open Reordered_argument_collections
open Typing_deps

let class_names_from_deps ~ctx ~get_classes_in_file deps =
  let filenames = Naming_provider.get_files ctx deps in
  Relative_path.Set.fold filenames ~init:SSet.empty ~f:(fun file acc ->
      SSet.fold (get_classes_in_file file) ~init:acc ~f:(fun cid acc ->
          if DepSet.mem deps Dep.(make (Type cid)) then
            SSet.add acc cid
          else
            acc))

let include_fanout_of_dep (mode : Mode.t) (dep : Dep.t) (deps : DepSet.t) :
    DepSet.t =
  let fanout = Typing_deps.get_ideps_from_hash mode dep in
  DepSet.union fanout deps

let get_minor_change_fanout
    ~(ctx : Provider_context.t)
    (class_dep : Dep.t)
    (member_diff : ClassDiff.member_diff) : DepSet.t =
  let mode = Provider_context.get_deps_mode ctx in
  let changed = DepSet.singleton class_dep in
  let acc = DepSet.singleton class_dep in
  let { consts; typeconsts; props; sprops; methods; smethods; constructor } =
    member_diff
  in
  let changed_and_descendants =
    lazy (Typing_deps.add_extend_deps mode changed)
  in
  (* Recheck any file with a dependency on the provided member
     in the changed class and in each of its descendants,
     even if the member was overridden in a subclass. This deals with the case
     where adding, removing, or changing the abstract-ness of a member causes
     some subclass which inherits a member of that name from multiple parents
     to resolve the conflict in a different way than it did previously. *)
  let recheck_descendants_and_their_member_dependents (acc : DepSet.t) member :
      DepSet.t =
    let changed_and_descendants = Lazy.force changed_and_descendants in
    DepSet.fold changed_and_descendants ~init:acc ~f:(fun dep acc ->
        DepSet.add acc dep
        |> include_fanout_of_dep
             mode
             (Typing_deps.Dep.make_member_dep_from_type_dep dep member))
  in
  let add_member_fanout
      ~is_const
      (member : Dep.Member.t)
      (change : member_change)
      (acc : DepSet.t) =
    (* Consts and typeconsts have their types copied into descendant classes in
       folded decl (rather than being stored in a separate heap as methods and
       properties are). As a result, when using a const, we register a
       dependency only upon the class type at the use site. In contrast, if we
       use a method or property B::f which was inherited from A, we register a
       dependency both upon B::f and A::f, which is what allows us to avoid the
       more expensive use of recheck_descendants_and_their_member_dependents
       here. Since we don't register a dependency upon the const at its class of
       origin in this way, we must always take the more expensive path for
       consts. *)
    if
      is_const || ClassDiff.method_or_property_change_affects_descendants change
    then
      recheck_descendants_and_their_member_dependents acc member
    else
      include_fanout_of_dep
        mode
        (Dep.make_member_dep_from_type_dep class_dep member)
        acc
  in
  let add_member_fanouts ~is_const changes make_member acc =
    SMap.fold changes ~init:acc ~f:(fun name ->
        add_member_fanout ~is_const (make_member name))
  in
  let acc =
    SMap.fold consts ~init:acc ~f:(fun name change acc ->
        let acc =
          (* If a const has been added or removed in an enum type, we must recheck
             all switch statements which need to have a case for each variant
             (exhaustive switch statements add an AllMembers dependency).
             We don't bother to test whether the class is an enum type because
             non-enum classes will have no AllMembers dependents anyway. *)
          match change with
          | Added
          | Removed ->
            recheck_descendants_and_their_member_dependents acc Dep.Member.all
          | _ -> acc
        in
        add_member_fanout ~is_const:true (Dep.Member.const name) change acc)
  in
  let acc =
    acc
    |> add_member_fanouts ~is_const:true typeconsts Dep.Member.const
    |> add_member_fanouts ~is_const:false props Dep.Member.prop
    |> add_member_fanouts ~is_const:false sprops Dep.Member.sprop
    |> add_member_fanouts ~is_const:false methods Dep.Member.method_
    |> add_member_fanouts ~is_const:false smethods Dep.Member.smethod
  in
  let acc =
    Option.value_map constructor ~default:acc ~f:(fun change ->
        add_member_fanout ~is_const:false Dep.Member.constructor change acc)
  in
  acc

let get_maximum_fanout (ctx : Provider_context.t) (class_dep : Dep.t) : DepSet.t
    =
  let mode = Provider_context.get_deps_mode ctx in
  Typing_deps.add_all_deps mode @@ DepSet.singleton class_dep

let get_fanout ~(ctx : Provider_context.t) (class_name, diff) : DepSet.t =
  let class_dep = Dep.make (Dep.Type class_name) in
  match diff with
  | Unchanged -> DepSet.make ()
  | Major_change _major_change -> get_maximum_fanout ctx class_dep
  | Minor_change minor_change ->
    get_minor_change_fanout ~ctx class_dep minor_change

let direct_references_cardinal mode class_name : int =
  Typing_deps.get_ideps mode (Dep.Type class_name) |> DepSet.cardinal

let descendants_cardinal mode class_name : int =
  (Typing_deps.add_extend_deps
     mode
     (DepSet.singleton @@ Dep.make @@ Dep.Type class_name)
  |> DepSet.cardinal)
  - 1

let children_cardinal mode class_name : int =
  Typing_deps.get_ideps mode (Dep.Extends class_name) |> DepSet.cardinal

module Log : sig
  val log_class_fanout :
    Provider_context.t -> string * ClassDiff.t -> DepSet.t -> unit

  val log_fanout :
    Provider_context.t ->
    'a list ->
    DepSet.t ->
    max_class_fanout_cardinal:int ->
    unit
end = struct
  let do_log ctx ~fanout_cardinal =
    TypecheckerOptions.log_fanout ~fanout_cardinal
    @@ Provider_context.get_tcopt ctx

  let log_class_fanout
      ctx ((class_name : string), (diff : ClassDiff.t)) (fanout : DepSet.t) :
      unit =
    let fanout_cardinal = DepSet.cardinal fanout in
    if do_log ~fanout_cardinal ctx then
      let mode = Provider_context.get_deps_mode ctx in
      HackEventLogger.Fanouts.log_class
        ~class_name
        ~class_diff:(ClassDiff.show diff)
        ~fanout_cardinal
        ~class_diff_category:(ClassDiff.to_category_json diff)
        ~direct_references_cardinal:(direct_references_cardinal mode class_name)
        ~descendants_cardinal:(descendants_cardinal mode class_name)
        ~children_cardinal:(children_cardinal mode class_name)

  let log_fanout
      ctx (changes : _ list) (fanout : DepSet.t) ~max_class_fanout_cardinal :
      unit =
    let fanout_cardinal = DepSet.cardinal fanout in
    if do_log ~fanout_cardinal ctx then
      HackEventLogger.Fanouts.log
        ~changes_cardinal:(List.length changes)
        ~max_class_fanout_cardinal
        ~fanout_cardinal
end

let add_fanout
    ~ctx ((fanout_acc, max_class_fanout_cardinal) : DepSet.t * int) diff :
    DepSet.t * int =
  let fanout = get_fanout ~ctx diff in
  Log.log_class_fanout ctx diff fanout;
  let fanout_acc = DepSet.union fanout_acc fanout in
  let max_class_fanout_cardinal =
    Int.max max_class_fanout_cardinal (DepSet.cardinal fanout)
  in
  (fanout_acc, max_class_fanout_cardinal)

let fanout_of_changes
    ~(ctx : Provider_context.t) (changes : (string * ClassDiff.t) list) :
    Fanout.t =
  let changed_deps =
    List.filter_map changes ~f:(fun (name, diff) ->
        Option.some_if (ClassDiff.has_changed diff) (Dep.make @@ Dep.Type name))
    |> DepSet.of_list
  in
  let (to_recheck, max_class_fanout_cardinal) =
    List.fold changes ~init:(DepSet.make (), 0) ~f:(add_fanout ~ctx)
  in
  Log.log_fanout ctx changes to_recheck ~max_class_fanout_cardinal;
  { Fanout.changed = changed_deps; to_recheck }
