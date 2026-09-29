use "format"

primitive _HealthCheckReporter
  fun report(
    params: PropertyParams,
    samples_run: USize,
    total_filter_discards: USize,
    total_filter_accepts: USize,
    max_choices: USize,
    peak_sample_nanos: U64,
    logger: _PropertyLogger)
    : USize
  =>
    var warnings: USize = 0

    if params.max_filter_discard_ratio > 0 then
      if total_filter_accepts > 0 then
        let ratio =
          total_filter_discards.f64() / total_filter_accepts.f64()
        if ratio > params.max_filter_discard_ratio then
          logger.log(
            "WARNING: filter discarded " +
              total_filter_discards.string() +
              " candidates across " + samples_run.string() +
              " samples (" +
              Format.float[F64](
                ratio where fmt = FormatFix, prec = 1) +
              "x discard ratio, threshold " +
              Format.float[F64](
                params.max_filter_discard_ratio
                  where fmt = FormatFix, prec = 1) +
              "x). Generator may be too narrow for the " +
              "filter predicate.")
          warnings = warnings + 1
        end
      end
    end

    if (params.max_choice_sequence_size > 0) and
      (max_choices > params.max_choice_sequence_size)
    then
      logger.log(
        "WARNING: largest choice sequence was " +
          max_choices.string() +
          " entries (threshold " +
          params.max_choice_sequence_size.string() +
          "). Consider simplifying the generator or reducing " +
          "collection sizes.")
      warnings = warnings + 1
    end

    if (params.max_sample_nanos > 0) and
      (peak_sample_nanos > params.max_sample_nanos)
    then
      let secs =
        peak_sample_nanos.f64() / 1_000_000_000.0
      let threshold_secs =
        params.max_sample_nanos.f64() / 1_000_000_000.0
      logger.log(
        "WARNING: slowest sample took " +
          Format.float[F64](
            secs where fmt = FormatFix, prec = 1) +
          "s (threshold " +
          Format.float[F64](
            threshold_secs where fmt = FormatFix, prec = 1) +
          "s). Consider reducing num_samples or simplifying " +
          "the property.")
      warnings = warnings + 1
    end

    warnings
