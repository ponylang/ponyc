use ".."
use "pony_test"
use "files"
use "json"

primitive _DiagnosticTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_DiagnosticTest)

class \nodoc\ iso _DiagnosticTest is UnitTest
  fun name(): String =>
    "diagnostics/compiler_error_to_diagnostics"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let workspace_dir = Path.join(Path.dir(__loc.file()), "error_workspace")
    let harness =
      TestHarness.create(
        h,
        PonyCompiler("", try h.env.args(0)? else "" end),
        object iso is MessageHandler
          var received_diagnostics: USize = 0
          let expected_messages:
            Array[String] val =
            [
              "argument not assignable to parameter"
              "argument type is String val"
              "parameter type requires U8 val^"
              "String val is not a subtype of U8 val^"
              "cannot infer type of fromb"
            ]

          fun handle_request(
            h: TestHelper,
            req: RequestMessage val,
            server: BaseProtocol)
          =>
            h.log("Received request: " + req.json().string())
            match req.method
            | Methods.workspace().configuration() =>
              // this will initialize the compiler
              server(
                ResponseMessage.create(req.id, JsonArray).string().iso_array())
              // send did_open notification to trigger compilation
              server(
                Notification(
                  Methods.text_document().did_open(),
                  JsonObject
                    .update(
                      "textDocument",
                      JsonObject
                        .update(
                          "uri",
                          Uris.from_path(Path.join(workspace_dir, "main.pony")))
                        .update("languageId", "pony")
                        .update("version", I64(1))
                        .update("text", "don't care"))
                ).string().iso_array()
              )
            end

          fun ref handle_notification(
            h: TestHelper,
            notification: Notification,
            server: BaseProtocol)
          =>
            h.log("received notification: " + notification.json().string())
            match notification.method
            | Methods.text_document().publish_diagnostics() =>
              try
                for diagnostic in
                  JsonNav(notification.params)("diagnostics")
                    .as_array()?.values()
                do
                  received_diagnostics = received_diagnostics + 1
                  h.log(
                    "received diagnostic " + received_diagnostics.string() +
                    ": " + notification.json().string())
                  // strip off linebreak from error message
                  let message: String val =
                    recover val
                      JsonNav(diagnostic)("message").as_string()?.clone()
                        .> strip()
                    end
                  h.assert_true(
                    expected_messages.contains(
                      message,
                      {(l, r) => l == r }),
                    "Unexpected diagnostic message: '" + message + "'")
                  if
                    received_diagnostics == 5
                  then
                    h.complete(true)
                  end
                end
              else
                h.fail(
                  "Weird diagnostics notification: " + notification.string())
              end
            end

          fun ref handle_response(
            h: TestHelper,
            res: ResponseMessage,
            server: BaseProtocol)
          =>
            h.log("received response: " + res.json().string())
            try
              h.assert_true(RequestIds.eq(I64(0), res.id as RequestId))
            else
              h.fail("No RequestId")
            end
            // send initialized notification
            server(
              Notification(Methods.initialized(), None).string().iso_array())
        end,
        {(h: TestHelper, harness: TestHarness ref): Bool => true }
        where
          after_sends = 3,
          after_logs = USize.max_value()
      )
    harness.send_to_server(LspMsg.initialize(workspace_dir))
