interface tag FetchNotify
  """
  Receives the result of a fetch operation.
  """

  be fetch_failed(err: FetchError)
    """
    Called when the fetch fails at any stage.
    """

  be fetch_succeeded()
    """
    Called when the archive has been downloaded and extracted.
    """
