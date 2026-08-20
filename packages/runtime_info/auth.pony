primitive SchedulerInfoAuth
  """
  Authorizes access to scheduler information.
  """
  new create(auth: AmbientAuth) => None

primitive ActorStatsAuth
  """
  Authorizes access to actor statistics.
  """
  new create(auth: AmbientAuth) => None

primitive SchedulerStatsAuth
  """
  Authorizes access to scheduler statistics.
  """
  new create(auth: AmbientAuth) => None
