use "pony_test"
use "buffered"
use "files"
use "json"
use ast = "pony_compiler"
use ".."

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    """
    Run all pony-lsp test suites.
    """
    test(_InitializeTest)
    test(_WorkspaceConfigurationTest)
    test(_DidChangeConfigurationTest)
    test(_DidChangeConfigurationNullTest)
    _WorkspaceTests.make().tests(test)
    _HoverFormatterTests.make().tests(test)
    _DiagnosticTests.make().tests(test)
    _HoverIntegrationTests.make().tests(test)

class \nodoc\ iso _InitializeTest is UnitTest
  fun name(): String => "initialize"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let harness =
      TestHarness.create(
        h,
        TestCompiler(h),
        object iso is MessageHandler
          fun handle_request(
            h: TestHelper,
            req: RequestMessage val,
            server: BaseProtocol)
          =>
            h.assert_eq[String](
              Methods.initialize(), req.method)
        end,
        {(h: TestHelper,
          harness: TestHarness ref): Bool =>
          let res =
            h.assert_eq[USize](
              1, harness.sent.size())
          h.complete(true)
          res
        }
        where
          after_sends = 1,
          after_logs = USize.max_value()
      )
    let workspace_dir =
      Path.join(
        Path.dir(__loc.file()), "workspace")
    harness.send_to_server(
      LspMsg.initialize(workspace_dir)
    )

class \nodoc\ iso _WorkspaceConfigurationTest
  is UnitTest
  fun name(): String =>
    Methods.workspace().configuration()

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    // ensure compiler got settings applied
    let compiler =
      TestCompiler(
        h
        where
          expected_defines = ["FOO"; "BAR"],
          expected_ponypath =
            ["/pony/path"; "/foo/bar"]
      )
    let harness =
      TestHarness.create(
        h,
        compiler,
        object iso is MessageHandler
          fun handle_response(
            h: TestHelper,
            res: ResponseMessage val,
            server: BaseProtocol)
          =>
            try
              h.assert_true(
                RequestIds.eq(
                  res.id as RequestId, 0))
            end
            // send initialized notification
            server(LspMsg.initialized())

          fun handle_request(
            h: TestHelper,
            req: RequestMessage val,
            server: BaseProtocol)
          =>
            // we expect a configuration request
            h.assert_eq[String](
              Methods.workspace()
                .configuration(),
              req.method
            )
            // send response
            server(
              ResponseMessage(
                req.id,
                JsonArray.push(
                  JsonObject
                    .update(
                      "defines",
                      JsonArray
                        .push("FOO")
                        .push("BAR"))
                    .update(
                      "ponypath",
                      JsonArray
                        .push("/pony/path")
                        .push("/foo/bar")))
              ).string().iso_array()
            )
        end,
        {(h: TestHelper,
          harness: TestHarness ref): Bool =>
          true
        }
      )
    let workspace_dir =
      Path.join(
        Path.dir(__loc.file()), "workspace")
    let init_msg =
      LspMsg.initialize(
        workspace_dir
        where
          did_change_configuration_dynamic_registration = false,
          supports_configuration = true
      )
    harness.send_to_server(consume init_msg)

class \nodoc\ iso _DidChangeConfigurationTest
  is UnitTest
  fun name(): String =>
    Methods.workspace()
      .did_change_configuration()

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    // ensure compiler got settings applied
    let compiler =
      TestCompiler(
        h
        where
          expected_defines = ["FOO"; "BAR"],
          expected_ponypath =
            ["/pony/path"; "/foo/bar"]
      )
    let harness =
      TestHarness.create(
        h,
        compiler,
        object iso is MessageHandler
          fun handle_response(
            h: TestHelper,
            res: ResponseMessage val,
            server: BaseProtocol)
          =>
            // this should be the initialize
            // response with server capabilities
            try
              h.assert_true(
                RequestIds.eq(
                  res.id as RequestId, 0))
            end
            // send initialized notification
            server(LspMsg.initialized())

          fun handle_request(
            h: TestHelper,
            req: RequestMessage val,
            server: BaseProtocol)
          =>
            // 1.) expect register capability
            h.assert_eq[String](
              Methods.client()
                .register_capability(),
              req.method)
            // 2.) send response with null result
            server(
              ResponseMessage(req.id, None)
                .into_bytes())
            // 3.) send didChangeConfiguration
            server(
              Notification(
                Methods.workspace()
                  .did_change_configuration(),
                JsonObject
                  .update(
                    "settings",
                    JsonObject
                      .update(
                        "pony-lsp",
                        JsonObject
                          .update(
                            "defines",
                            JsonArray
                              .push("FOO")
                              .push("BAR"))
                          .update(
                            "ponypath",
                            JsonArray
                              .push("/pony/path")
                              .push("/foo/bar"))
                      )
                  )
              ).into_bytes()
            )
            None
        end,
        {(h: TestHelper,
          harness: TestHarness ref): Bool =>
          true
        }
      )

    let workspace_dir =
      Path.join(
        Path.dir(__loc.file()), "workspace")
    harness.send_to_server(
      LspMsg.initialize(
        workspace_dir
        where
          did_change_configuration_dynamic_registration = true,
          supports_configuration = false
      )
    )

class \nodoc\ iso
  _DidChangeConfigurationNullTest
  is UnitTest
  fun name(): String =>
    Methods.workspace()
      .did_change_configuration() + "/null"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    // ensure compiler got settings applied
    let compiler =
      TestCompiler(
        h
        where
          expected_defines = ["FOO"; "BAR"],
          expected_ponypath =
            ["/pony/path"; "/foo/bar"]
      )
    let harness =
      TestHarness.create(
        h,
        compiler,
        object iso is MessageHandler
          var num_conf_requests: USize = 0

          fun handle_response(
            h: TestHelper,
            res: ResponseMessage val,
            server: BaseProtocol)
          =>
            // this should be the initialize
            // response with server capabilities
            try
              h.assert_true(
                RequestIds.eq(
                  res.id as RequestId, 0))
            end
            // send initialized notification
            server(LspMsg.initialized())

          fun ref handle_request(
            h: TestHelper,
            req: RequestMessage val,
            server: BaseProtocol)
          =>
            match req.method
            | Methods.client()
              .register_capability()
            =>
              // 1.) expect register capability
              h.assert_eq[String](
                Methods.client()
                  .register_capability(),
                req.method)
              // 2.) send response with null result
              server(
                ResponseMessage(req.id, None)
                  .into_bytes())
              // 3.) send empty
              // didChangeConfiguration with null
              server(
                Notification(
                  Methods.workspace()
                    .did_change_configuration(),
                  JsonObject
                    .update("settings", None)
                ).into_bytes()
              )
            | Methods.workspace()
              .configuration()
            =>
              if num_conf_requests == 0 then
                // send empty settings at first
                server(
                  ResponseMessage(
                    req.id, JsonArray)
                    .into_bytes())
              else
                // this request should be
                // triggered by the null
                // didChangeConfiguration above
                server(
                  ResponseMessage(
                    req.id,
                    JsonArray.push(
                      JsonObject
                        .update(
                          "defines",
                          JsonArray
                            .push("FOO")
                            .push("BAR"))
                        .update(
                          "ponypath",
                          JsonArray
                            .push("/pony/path")
                            .push("/foo/bar")))
                  ).into_bytes()
                )
              end
              num_conf_requests =
                num_conf_requests + 1
            end
            None
        end,
        {(h: TestHelper,
          harness: TestHarness ref): Bool =>
          true
        }
      )

    let workspace_dir =
      Path.join(
        Path.dir(__loc.file()), "workspace")
    harness.send_to_server(
      LspMsg.initialize(
        workspace_dir
        where
          did_change_configuration_dynamic_registration = true,
          supports_configuration = true
      )
    )
