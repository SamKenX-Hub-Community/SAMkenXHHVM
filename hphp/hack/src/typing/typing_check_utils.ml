(*
 * Copyright (c) 2015, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

open Hh_prelude

let type_file
    tcopt
    fn
    {
      FileInfo.funs;
      classes;
      typedefs;
      consts;
      modules;
      hash = _;
      file_mode = _;
      comments = _;
    } =
  let snd (_, x, _) = x in
  let (errors, tast) =
    Errors.do_with_context fn Errors.Typing (fun () ->
        let check ~f defs =
          List.map defs ~f:snd |> List.filter_map ~f:(fun x -> f tcopt fn x)
        in
        let fs = check ~f:Typing_check_job.type_fun funs |> List.concat in
        let cs = check ~f:Typing_check_job.type_class classes in
        let ts = check ~f:Typing_check_job.check_typedef typedefs in
        let gcs = check ~f:Typing_check_job.check_const consts in
        let mds = check ~f:Typing_check_job.check_module modules in
        fs @ cs @ ts @ gcs @ mds)
  in
  (tast, errors)
