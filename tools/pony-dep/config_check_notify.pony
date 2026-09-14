trait tag ConfigCheckNotify
  """
  Receives the result of a `ConfigAccess.check_name` query.
  """

  be dep_name_available()
    """
    Called when the dep name is not already in the config.
    """

  be dep_name_rejected(message: String val)
    """
    Called when the dep name cannot be added.
    """

trait tag ConfigReadNotify
  """
  Receives the result of a `ConfigAccess.read_config` query.
  """

  be config_loaded(config: ConfigFile val)
    """
    Called when the config file has been read and parsed.
    """

  be config_not_found()
    """
    Called when the config file does not exist.
    """

  be config_error(message: String val)
    """
    Called when the config file cannot be read or parsed.
    """

trait tag ConfigWriteNotify
  """
  Receives the result of a `ConfigAccess.add_entry` write.
  """

  be dep_added()
    """
    Called when the dep has been written to the config file.
    """

  be dep_add_failed(message: String val)
    """
    Called when the dep could not be written to the config file.
    """
