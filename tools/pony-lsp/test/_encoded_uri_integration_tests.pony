use ".."
use "files"
use "json"
use "pony_test"

primitive \nodoc\ _EncodedURIIntegrationTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_EncodedURIRoutesToWorkspaceTest)

primitive \nodoc\ _ClientSpelling
  """
  Spells a path the way an LSP client does, without going through `Uris`.

  A test that built its URIs with `Uris.from_path` would still pass if
  encoding and decoding were broken in matching ways, because the two would
  cancel out. Spelling the URI here keeps the client's side independent of the
  code under test.

  The Windows spelling is VS Code's: forward slashes, and an escaped colon
  after the drive letter.
  """
  fun uri(path: String): String =>
    let s = path.clone()
    ifdef windows then
      s.replace("\\", "/")
      s.replace(":", "%3A")
    end
    s.replace(" ", "%20")
    let prefix = ifdef windows then "file:///" else "file://" end
    prefix + consume s

class \nodoc\ iso _EncodedURIRoutesToWorkspaceTest is UnitTest
  """
  A document URI carrying a percent-escape has to reach its workspace.

  A client names the document with an escape. If the escape survives to_path,
  the router matches the escaped text against no workspace path, and the
  request comes back as an error.
  """
  fun name(): String => "uris/integration/encoded_uri_routes_to_workspace"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let workspace_dir =
      Path.join(Path.dir(__loc.file()), "workspace with space")
    let workspace_uri = _ClientSpelling.uri(workspace_dir)
    let document_uri =
      _ClientSpelling.uri(Path.join(workspace_dir, "main.pony"))

    // A fixture path with no space would leave nothing for to_path to decode.
    h.assert_true(
      workspace_uri.contains("%20"),
      "expected an escaped workspace URI, got " + workspace_uri)
    ifdef windows then
      h.assert_true(
        workspace_uri.contains("%3A"),
        "expected an escaped drive colon, got " + workspace_uri)
    end
    // _ClientSpelling escapes a space and a drive colon and nothing else, so a
    // checkout path holding a '%' would spell a URI for a different directory.
    h.assert_false(
      workspace_dir.contains("%"),
      "checkout path needs escaping this test does not do: " + workspace_dir)

    let harness =
      TestHarness.create(
        h,
        TestCompiler(h),
        object iso is MessageHandler
          var _seen: USize = 0

          fun ref handle_response(
            h: TestHelper,
            res: ResponseMessage val,
            server: BaseProtocol)
          =>
            _seen = _seen + 1
            if _seen == 1 then
              // The initialize response.
              server(LspMsg.initialized())
              server(
                RequestMessage(
                  I64(300),
                  Methods.text_document().document_symbol(),
                  JsonObject.update(
                    "textDocument",
                    JsonObject.update("uri", document_uri)))
                .into_bytes())
            elseif _seen == 2 then
              match \exhaustive\ res.err
              | let e: ResponseError val =>
                h.fail("no workspace matched the document URI: " + e.message)
              | None =>
                None
              end
              h.complete(true)
            end
        end,
        {(h: TestHelper, harness: TestHarness ref): Bool => true }
        where
          after_sends = USize.max_value(),
          after_logs = USize.max_value()
      )
    harness.send_to_server(
      LspMsg.initialize(workspace_dir where uri = workspace_uri))
