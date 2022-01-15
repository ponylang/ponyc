use "cli"
use "files"

primitive _FindExecutable
  fun apply(env: Env, name: String): FilePath ? =>
    if Path.is_abs(name) then
      FilePath(env.root, name)
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
          let dir_file_path = FilePath(env.root, dir)
          ifdef windows then
            var bin_file_path: FilePath
            try
              bin_file_path = FilePath.from(dir_file_path, name)?
              if bin_file_path.exists() then return bin_file_path end
            end
            bin_file_path = FilePath.from(dir_file_path, name + ".exe")?
            if bin_file_path.exists() then return bin_file_path end
          else
            let bin_file_path = FilePath.from(dir_file_path, name)?
            if bin_file_path.exists() then return bin_file_path end
          end
        end
      end
      error
    end
