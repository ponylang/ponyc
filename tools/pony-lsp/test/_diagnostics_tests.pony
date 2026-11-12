use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive \nodoc\ _DiagnosticTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_DiagnosticTest)
    test(_CompilerDiagnosticTest)
    test(recover iso WsTest(_FilelessDiagnosticTest(true, false)) end)
    test(recover iso WsTest(_FilelessDiagnosticTest(false, true)) end)
    test(recover iso WsTest(_FilelessDiagnosticTest(false, false)) end)

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
          let seen: Set[String] iso = recover iso Set[String] end
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
            h.log("Received request: " + req.json().print())
            match req.method
            | Methods.workspace().configuration() =>
              // this will initialize the compiler
              server(
                ResponseMessage.create(req.id, JSONArray).string().clone().iso_array())
              // send did_open notification to trigger compilation
              server(
                Notification(
                  Methods.text_document().did_open(),
                  JSONObject
                    .update(
                      "textDocument",
                      JSONObject
                        .update(
                          "uri",
                          Uris.from_path(Path.join(workspace_dir, "main.pony")))
                        .update("languageId", "pony")
                        .update("version", I64(1))
                        .update("text", "don't care"))
                ).string().clone().iso_array()
              )
            end

          fun ref handle_notification(
            h: TestHelper,
            notification: Notification,
            server: BaseProtocol)
          =>
            h.log("received notification: " + notification.json().print())
            match notification.method
            | Methods.text_document().publish_diagnostics() =>
              try
                for diagnostic in
                  JSONNav(notification.params)("diagnostics")
                    .as_array()?.values()
                do
                  received_diagnostics = received_diagnostics + 1
                  h.log(
                    "received diagnostic " + received_diagnostics.string() +
                    ": " + notification.json().print())
                  // strip off linebreak from error message
                  let message: String val =
                    recover val
                      JSONNav(diagnostic)("message").as_string()?.clone()
                        .> strip()
                    end
                  h.assert_true(
                    expected_messages.contains(
                      message,
                      {(l, r) => l == r }),
                    "Unexpected diagnostic message: '" + message + "'")
                  h.assert_false(seen.contains(message), message)
                  seen.set(message)
                  let is_parameter = message == "parameter type requires U8 val^"
                  let file = if is_parameter then "frombnicator.pony"
                    else "main.pony" end
                  h.assert_eq[String](
                    Uris.from_path(Path.join(workspace_dir, file)),
                    JSONNav(notification.params)("uri").as_string()?)
                  let range = JSONNav(diagnostic)("range")
                  let line: I64 = if is_parameter then 6
                    elseif message == "cannot infer type of fromb" then 4
                    else 2 end
                  let column: I64 = if is_parameter then 13
                    elseif message == "cannot infer type of fromb" then 6
                    else 36 end
                  h.assert_eq[I64](line, range("start")("line").as_i64()?)
                  h.assert_eq[I64](column,
                    range("start")("character").as_i64()?)
                  if
                    received_diagnostics == 5
                  then
                    server(
                      RequestMessage(
                        I64(999), Methods.text_document().diagnostic(),
                        JSONObject.update("textDocument",
                          JSONObject.update("uri", Uris.from_path(
                            Path.join(workspace_dir, "main.pony")))))
                      .into_bytes())
                  end
                end
              else
                h.fail(
                  "Weird diagnostics notification: " + notification.string())
              end
            | Methods.window().show_message() =>
              h.fail("A file-backed error became a window message")
            end

          fun ref handle_response(
            h: TestHelper,
            res: ResponseMessage,
            server: BaseProtocol)
          =>
            h.log("received response: " + res.json().print())
            try
              let id = res.id as RequestId
              if RequestIds.eq(I64(999), id) then
                h.assert_eq[USize](5, received_diagnostics)
                h.assert_eq[USize](5, seen.size())
                h.complete(true)
              else
                h.assert_true(RequestIds.eq(I64(0), id))
                server(LspMsg.initialized())
              end
            else
              h.fail("No RequestId")
            end
        end,
        {(h: TestHelper, harness: TestHarness ref): Bool => true }
        where
          after_sends = 3,
          after_logs = USize.max_value()
      )
    harness.send_to_server(LspMsg.initialize(workspace_dir))
