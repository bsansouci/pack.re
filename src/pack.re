
open Types;

let cat = (filename) => {
  let ic = open_in_bin(filename);
  let len = in_channel_length(ic);
  let buf = Buffer.create(len);
  Buffer.add_channel(buf, ic, len);
  let content = Buffer.contents(buf);
  close_in(ic);
  content
};

let abspath = (path) => {
  let path = if (path.[0] == '.') {
    Filename.concat(Unix.getcwd(), path)
  } else {
    path
  };
  let parts = Str.split(Str.regexp("/"), path);
  let parts = ["", ...parts];
  let rec loop = (items) => switch items {
  | [] => []
  | [".", ...rest] => loop(rest)
  | ["..", ...rest] => ["..", ...loop(rest)]
  | [_, "..", ...rest] => loop(rest)
  | [item, ...rest] => [item, ...loop(rest)]
  };
  let rec chug = (items) => {
    let next = loop(items);
    if (next != items) {
      chug(next)
    } else {
      items
    }
  };
  String.concat(Filename.dir_sep, chug(parts))
};

let rec process = (state, path) => {
  let path = abspath(path);
  if (Hashtbl.mem(state.ids, path)) {
    Hashtbl.find(state.ids, path);
  } else {
    state.nextId = state.nextId + 1;
    let id = state.nextId;
    Hashtbl.add(state.ids, path, id);
    let contents = cat(path);
    let requires = FindRequires.parseContents(path, contents);
    /* let requires = []; */
    let fixed = FixFile.process(state, path, contents, requires, process);
    state.modules = [(id, path, fixed), ...state.modules];
    id
  }
};

let mapOf = List.fold_left((m, (a, b)) => StrMap.add(a, b, m), StrMap.empty);

let process = (abspath) => {
  let state = {
    entry: abspath,
    alias: mapOf([
      ("Reprocessing", "@jaredly/reprocessing"),
      ("ReasonglWeb", "@jaredly/reasongl-web"),
      ("ReasonglInterface", "@jaredly/reasongl-interface"),
    ]),
    nodeModulesBase: "../../games/gravitron/node_modules",
    ids: Hashtbl.create(100),
    nextId: 0,
    modules: []
  };
  process(state, abspath) |> ignore;
  {|
    ;(function() {
      let modules = {}
      let initializers = {
    |} ++ (List.map(
    ((id, path, body)) => string_of_int(id) ++ ": function(module, exports, require) { // " ++ path ++ "\n" ++  body ++ "}",
    state.modules
  ) |> String.concat(",\n\n"))
  ++ {|
      };
      let require = (id) => {
        if (!modules[id]) {
          modules[id] = {exports: {}}
          initializers[id](modules[id], modules[id].exports, require)
        }
        return modules[id].exports
      };
      require(1)
    })();
  |}
};

process(Sys.argv[1])
|> print_endline