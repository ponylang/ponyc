trait tag ConfigChecker
  """
  Checks whether a dependency name can be added to the configuration.
  """

  be check_name(
    name: String val,
    notify: ConfigCheckNotify)
    """
    Reads the config and checks whether `name` is available.
    """

trait tag ConfigReader
  """
  Reads and parses the configuration file.
  """

  be read_config(notify: ConfigReadNotify)
    """
    Reads and parses the config file.
    """

trait tag ConfigAdder
  """
  Adds a new dependency entry to the configuration file.
  """

  be add_entry(
    name: String val,
    dep_type: String val,
    url: String val,
    hash: String val,
    ref_name: (String val | None),
    documentation_url: (String val | None),
    notify: ConfigWriteNotify)
    """
    Appends a dependency entry to the config file.
    """
