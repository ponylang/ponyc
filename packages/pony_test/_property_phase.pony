
trait _PropertyPhase
  """
  One phase of the property test lifecycle. Async completion events
  dispatch through the current phase so each handles completions in
  its own way.
  """

  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)

  fun expect_action(
    name: String,
    runner: _TestRunner ref)

primitive _PropertyIdle is _PropertyPhase
  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    None

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    None

  fun expect_action(
    name: String,
    runner: _TestRunner ref)
  =>
    None

primitive _SyncSampling is _PropertyPhase
  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    runner._log(
      "async sample complete during sync sampling, discarding", true)

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    runner._log(
      "async action complete during sync sampling, discarding", true)

  fun expect_action(
    name: String,
    runner: _TestRunner ref)
  =>
    runner._log(
      "expect_action during sync sampling, discarding", true)

primitive _AsyncSampling is _PropertyPhase
  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    if runner._check_regression_replay(success, exec) then return end
    if not success then
      runner._async_sample_failed(exec)
    else
      exec.sample_passed()
      runner._next_property_sample()
    end

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    if not success then
      runner._log("Action failed: " + name, false)
      sample_complete(false, exec, runner)
      return
    end

    runner._log("Action completed: " + name, true)

    if runner._remove_prop_action(name) then
      if not runner._has_pending_actions() then
        sample_complete(true, exec, runner)
      end
    else
      runner._log_unexpected_action(name)
    end

  fun expect_action(
    name: String,
    runner: _TestRunner ref)
  =>
    runner._add_prop_action(name)

primitive _SyncShrinking is _PropertyPhase
  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    runner._log(
      "async sample complete during sync shrinking, discarding", true)

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    runner._log(
      "async action complete during sync shrinking, discarding", true)

  fun expect_action(
    name: String,
    runner: _TestRunner ref)
  =>
    runner._log(
      "expect_action during sync shrinking, discarding", true)

primitive _AsyncShrinking is _PropertyPhase
  fun sample_complete(
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    if not success then
      runner._shrink_candidate_failed()
    end
    runner._advance_shrink_step()

  fun action_complete(
    name: String,
    success: Bool,
    exec: _PropertyExecution,
    runner: _TestRunner ref)
  =>
    if not success then
      runner._log("Action failed: " + name, false)
      sample_complete(false, exec, runner)
      return
    end

    runner._log("Action completed: " + name, true)

    if runner._remove_prop_action(name) then
      if not runner._has_pending_actions() then
        sample_complete(true, exec, runner)
      end
    else
      runner._log_unexpected_action(name)
    end

  fun expect_action(
    name: String,
    runner: _TestRunner ref)
  =>
    runner._add_prop_action(name)
