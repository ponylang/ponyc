use "cli"
use "files"

primitive _FindExecutable
  fun apply(env: Env, name: String): FilePath ? =>
    let fname =
      if try name(0)? == '"' else false end then
        name.trim(1, name.size() - 1)
      else
        name
      end

    if Path.is_abs(fname) then
      FilePath(FileAuth(env.root), fname)
    else
      (let vars, let key) =
        ifdef windows then
          (EnvVars(env.vars, "", true), "path")
        else
          (EnvVars(env.vars), "PATH")
        end
      let path_var = vars.get_or_else(key, "")
      for dir in Path.split_list(path_var).values() do
        try
          let dir_file_path = FilePath(FileAuth(env.root), dir)
          ifdef windows then
            var bin_file_path: FilePath
            try
              bin_file_path = FilePath.from(dir_file_path, fname)?
              if bin_file_path.exists() then return bin_file_path end
            end
            bin_file_path = FilePath.from(dir_file_path, fname + ".exe")?
            if bin_file_path.exists() then return bin_file_path end
          else
            let bin_file_path = FilePath.from(dir_file_path, fname)?
            if bin_file_path.exists() then return bin_file_path end
          end
        end
      end
      error
    end
