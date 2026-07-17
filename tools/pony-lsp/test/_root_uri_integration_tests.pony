use ".."
use "files"
use "json"
use "pony_test"

primitive \nodoc\ _RootURIIntegrationTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_RootURIRoutesToWorkspaceTest)
    test(_RootPathRoutesToWorkspaceTest)

class \nodoc\ iso _RootURIRoutesToWorkspaceTest is UnitTest
  """
  A client that names the workspace with rootUri and sends no workspaceFolders
  has to reach its workspace.

  The initialize params carry the workspace location under one of three keys.
  Only workspaceFolders holds an array; rootUri and rootPath are both strings,
  and rootUri is a URI. A URI reaching the scanner without conversion names no
  directory, so no workspace registers and every document request comes back
  as an error.
  """
  fun name(): String => "uris/integration/root_uri_routes_to_workspace"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let workspace_uri = _ClientSpelling.uri(workspace_dir)
    let document_uri =
      _ClientSpelling.uri(Path.join(workspace_dir, "main.pony"))

    // A URI with no scheme would leave nothing for to_path to strip.
    h.assert_true(
      workspace_uri.contains("://"),
      "expected a URI with a scheme, got " + workspace_uri)
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
                  I64(400),
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
    // vim-lsp's default configuration: rootUri and rootPath, no
    // workspaceFolders. The extraction prefers rootUri over rootPath, so the
    // URI is what reaches the scanner.
    harness.send_to_server(
      LspMsg.initialize(
        workspace_dir
        where
          uri = workspace_uri,
          send_workspace_folders = false))

class \nodoc\ iso _RootPathRoutesToWorkspaceTest is UnitTest
  """
  A client that names the workspace with rootPath alone has to reach its
  workspace.

  rootPath is a path, not a URI, and it reaches the scanner through the same
  conversion as rootUri. This test fails if that conversion stops passing a
  plain path through.
  """
  fun name(): String => "uris/integration/root_path_routes_to_workspace"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let document_uri =
      _ClientSpelling.uri(Path.join(workspace_dir, "main.pony"))

    // A workspace path holding a scheme separator would not survive the
    // conversion this test covers.
    h.assert_false(
      workspace_dir.contains("://"),
      "checkout path contains a scheme separator: " + workspace_dir)
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
                  I64(401),
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
      LspMsg.initialize(
        workspace_dir
        where
          send_workspace_folders = false,
          send_root_uri = false))
