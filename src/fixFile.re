
let sliceToEnd = (str, pos) => String.sub(str, pos, String.length(str) - pos);

let rec firstPart = parts => {
  let d = Filename.dirname(parts);
  if (d == ".") {
    parts
  } else {
    firstPart(d)
  }
};

let exists = path => try {Unix.stat(path) |> ignore; true} {
| Unix.Unix_error(Unix.ENOENT, _, _) => false
| Unix.Unix_error(Unix.ENOTDIR, _, _) => false
};

let rec findNodeModule = (needle, base) => {
  if (exists(base) && ReasonCliTools.Files.isDirectory(base)) {
    let full = Filename.concat(base, needle);
    if (ReasonCliTools.Files.isDirectory(full)) {
      Some(full)
    } else {
      let names = ReasonCliTools.Files.readDirectory(base);
      let rec loop = names => {
        switch (names) {
        | [name, ...rest] => {
          let child = name.[0] == '@' ? name : Filename.concat(name, "node_modules");
          switch (findNodeModule(needle, Filename.concat(base, child))) {
          | None => loop(rest)
          | Some(x) => Some(x)
          }
        }
        | [] => None
      }
      };
      loop(names)
    }
  } else {
    None
  }
};

open Types;

open ReasonCliTools;

let (|>>) = (fn, v) => switch v { | None => None | Some(v) => fn(v) };
let unwrap = (message, v) => switch v { | None => failwith(message) | Some(v) => v };

let resolvePackageJsonMain = foundPath => {
  let contents = ReasonCliTools.Files.readFile(Filename.concat(foundPath, "package.json"));
  let data = Json.parse(contents |> unwrap("Unable to read package.json " ++ foundPath));
  switch (data |> Json.get("main")) {
  | Some(Json.String(v)) => Filename.concat(foundPath, v)
  | _ => failwith("No main found in package.json " ++ foundPath)
  }
};

let resolve = (state, base, path) => {
  if (String.length(path) == 0) {
    failwith("Invalid require - empty string")
  } else {
    let foundPath = if (path.[0] == '.') {
      Filename.concat(Filename.dirname(base), path)
    } else {
      let moduleName = firstPart(path);
      /* print_endline(moduleName ++ ":" ++ path); */
      let rest = moduleName != path ? sliceToEnd(path, String.length(moduleName) + 1) : "";
      let moduleName = if (StrMap.mem(moduleName, state.alias)) {
        StrMap.find(moduleName, state.alias)
      } else {
        moduleName
      };
      let base = switch (findNodeModule(moduleName, Filename.concat(state.base, "node_modules"))) {
      | None => failwith("Node module not found: " ++ moduleName)
      | Some(x) => x
      };
      Filename.concat(base, rest);
    };
    if (Files.isDirectory(foundPath)) {
      if (Files.isFile(Filename.concat(foundPath, "package.json"))) {
        resolvePackageJsonMain(foundPath)
      } else if (Files.isFile(Filename.concat(foundPath, "index.js"))) {
        Filename.concat(foundPath, "index.js")
      } else {
        failwith("Directory has no discernable default js file: " ++ foundPath)
      }
    } else if (Files.isFile(foundPath)) {
      foundPath
    } else if (Files.isFile(foundPath ++ ".js")) {
      foundPath ++ ".js"
    } else {
      failwith("Not a real thing: " ++ foundPath)
    }
  }
};

Printexc.record_backtrace(true);

let process = (state, abspath, contents, requires, loop) => {
  let (fixed, _) = requires |> List.fold_left(
    ((contents, offset), {pos, length, text}) => {
      /* print_endline(string_of_int(pos) ++ ":" ++ string_of_int(offset)); */
      let pos = offset + pos;
      /* print_endline(string_of_int(pos)); */
      let pre = String.sub(contents, 0, pos);
      let post = sliceToEnd(contents, pos + length);
      let childPath = resolve(state, abspath, text);
      let childId = loop(state, childPath);
      let strId = string_of_int(childId);
      (
        pre ++ strId ++ post,
        offset + (String.length(strId) - length)
      )
    },
    (contents, 0)
  );
  fixed
};
