type RuntimeInfoAuth is (AmbientAuth | SchedulerInfoAuth)

primitive SchedulerInfoAuth
  new create(auth: AmbientAuth) => None
