(*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)
open Hh_prelude

let find ~entry ~(range : Lsp.range) ctx =
  if Lsp_helpers.lsp_range_is_selection range then
    let source_text = Ast_provider.compute_source_text ~entry in
    let line_to_offset line =
      Full_fidelity_source_text.position_to_offset source_text (line, 0)
    in
    let path = entry.Provider_context.path in
    let selection = Lsp_helpers.lsp_range_to_pos ~line_to_offset path range in
    Extract_classish_find_candidate.find_candidate ~selection entry ctx
    |> Option.map
         ~f:(Extract_classish_to_refactors.to_refactors source_text path)
    |> Option.value ~default:[]
  else
    []
