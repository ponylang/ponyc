use "files"
use "logger"

class val _TestDefinition
  let path: String
  let expected_exit_code: ISize

primitive _TestDefinitions
  fun apply(auth: (AmbientAuth | FileAuth), path: String,
    logger: Logger[String]) : (Array[_TestDefinition] val | None)
  =>
    logger(Info) and logger.log("Discovering tests in " + path)

    let fp =
      try
        FilePath(auth, path)?
      else
        logger(Error) and logger.log("Unable to open: " + path)
        return
      end

    if not fp.exists() then
      logger(Error) and logger.log("Path does not exist: " + path)
      return
    end

    let fi =
      try
        FileInfo(fp)?
      else
        logger(Error) and logger.log("Unable to get info: " + path)
        return
      end

    if not fi.directory then
      logger(Error) and logger.log("Not a directory: " + path)
      return
    end

    let dir =
      try
        Directory(fp)?
      else
        logger(Error) and logger.log("Unable to open directory: " + path)
        return
      end

    let entries = dir.entries()

    recover
      let definitions: Array[_TestDefinition] ref = Array[_TestDefinition]
      for entry in consume entries do
        match _get_definition(fp, entry)
        | let def: _TestDefinition => definitions.push(def)
        end
      end
      definitions
    end

  fun _get_definition(fp: FilePath, entry: String)
    : (_TestDefinition | String)
  =>
