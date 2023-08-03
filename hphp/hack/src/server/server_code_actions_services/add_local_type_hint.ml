open Hh_prelude

type candidate = {
  lhs_var: string;
  lhs_type: string;
  lhs_pos: Pos.t;
}

let should_offer_refactor ~(selection : Pos.t) ~lhs_pos ~rhs_pos =
  let contains_full_assignment =
    Pos.contains selection rhs_pos && Pos.contains selection lhs_pos
  in
  contains_full_assignment || Pos.contains lhs_pos selection

let find_candidate ~(selection : Pos.t) ~entry ctx : candidate option =
  let { Tast_provider.Compute_tast.tast; _ } =
    Tast_provider.compute_tast_quarantined ~ctx ~entry
  in
  let visitor =
    object
      inherit [candidate option] Tast_visitor.reduce as super

      method zero = None

      method plus = Option.first_some

      method! on_class_ env class_ =
        let pos = class_.Aast_defs.c_span in
        if Pos.contains pos selection then
          super#on_class_ env class_
        else
          None

      method! on_method_ env meth =
        let pos = Aast_defs.(meth.m_span) in
        if Pos.contains pos selection then
          super#on_method_ env meth
        else
          None

      method! on_fun_def env fd =
        let pos = Aast_defs.(fd.fd_fun.f_span) in
        if Pos.contains pos selection then
          super#on_fun_def env fd
        else
          None

      method! on_stmt env stmt =
        let (pos, stmt_) = stmt in
        if Pos.contains pos selection then
          let open Aast in
          match stmt_ with
          | Expr
              ( _,
                _,
                Binop
                  {
                    bop = Ast_defs.Eq None;
                    lhs = (lvar_ty, lhs_pos, Lvar (lid_pos, lid));
                    rhs = (_, rhs_pos, _);
                  } )
            when should_offer_refactor ~selection ~lhs_pos ~rhs_pos ->
            let tenv = Tast_env.tast_env_as_typing_env env in
            Some
              {
                lhs_var = Local_id.get_name lid;
                lhs_type = Typing_print.full_strip_ns tenv lvar_ty;
                lhs_pos = lid_pos;
              }
          | _ -> super#on_stmt env stmt
        else
          None
    end
  in
  visitor#go ctx tast.Tast_with_dynamic.under_normal_assumptions

let edit_of_candidate ~path { lhs_var; lhs_type; lhs_pos } : Lsp.WorkspaceEdit.t
    =
  let edit =
    let range =
      Lsp_helpers.hack_pos_to_lsp_range ~equal:Relative_path.equal lhs_pos
    in
    let text = Printf.sprintf "let %s : %s " lhs_var lhs_type in
    Lsp.TextEdit.{ range; newText = text }
  in
  let changes = SMap.singleton (Relative_path.to_absolute path) [edit] in
  Lsp.WorkspaceEdit.{ changes }

let to_refactor ~path candidate =
  let edit = lazy (edit_of_candidate ~path candidate) in
  let title = Printf.sprintf "Add local type hint for %s" candidate.lhs_var in
  Code_action_types.Refactor.{ title; edit }

let has_typed_local_variables_enabled root_node =
  let open Full_fidelity_positioned_syntax in
  let skip_traversal n =
    Full_fidelity_positioned_syntax.(
      is_classish_declaration n
      || is_classish_body n
      || is_methodish_declaration n
      || is_methodish_trait_resolution n
      || is_function_declaration n
      || is_function_declaration_header n)
  in
  let has_file_attr kwrd attrs =
    String.equal kwrd "file"
    && String.is_substring attrs ~substring:"EnableUnstableFeatures"
    && String.is_substring attrs ~substring:"typed_local_variables"
  in
  let rec aux nodes =
    match nodes with
    | [] -> false
    | [] :: nss -> aux nss
    | (n :: ns) :: nss ->
      (match n.syntax with
      | FileAttributeSpecification r ->
        if
          has_file_attr
            (text r.file_attribute_specification_keyword)
            (text r.file_attribute_specification_attributes)
        then
          true
        else
          aux (ns :: nss)
      | _ ->
        if skip_traversal n then
          aux (ns :: nss)
        else
          aux (children n :: ns :: nss))
  in
  aux [[root_node]]

let find ~entry ~(range : Lsp.range) ctx =
  let source_text = Ast_provider.compute_source_text ~entry in
  let cst = Ast_provider.compute_cst ~ctx ~entry in
  let root_node = Provider_context.PositionedSyntaxTree.root cst in
  if has_typed_local_variables_enabled root_node then
    let line_to_offset line =
      Full_fidelity_source_text.position_to_offset source_text (line, 0)
    in
    let path = entry.Provider_context.path in
    let selection = Lsp_helpers.lsp_range_to_pos ~line_to_offset path range in
    find_candidate ~selection ~entry ctx
    |> Option.map ~f:(to_refactor ~path)
    |> Option.to_list
  else
    []
