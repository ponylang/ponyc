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
    test(_InitializeTest)
    test(_WorkspaceConfigurationTest)
    test(_DidChangeConfigurationTest)
    _WorkspaceTests.make().tests(test)
    _HoverTests.make().tests(test)

primitive _LspMsg
  fun tag apply(msg: String val): Array[U8] iso^ =>
    let len = msg.size()
    let s = recover iso String.create(len + 25)
        .> append("Content-Length: ")
        .> append(len.string())
        .> append("\r\n\r\n")
        .> append(msg)
      end
    (consume s).iso_array()

  fun tag initialized(): Array[U8] iso^ =>
    this.apply("""
    {
      "jsonrpc": "2.0",
      "method": "initialized",
      "params": {}
    }
    """)

  fun tag initialize(
    dir: String,
    did_change_configuration_dynamic_registration: Bool = true,
    supports_configuration: Bool = true
  ): Array[U8] iso^ =>
    this.apply("""
    {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "initialize",
      "params": {
          "processId": 29381,
          "clientInfo": {
              "name": "Visual Studio Code",
              "version": "1.88.0"
          },
          "locale": "en",
          "rootPath": """" + dir + """
",
          "rootUri": "file://""" + dir + """
",
          "capabilities": {
              "workspace": {
                  "applyEdit": true,
                  "workspaceEdit": {
                      "documentChanges": true,
                      "resourceOperations": [
                          "create",
                          "rename",
                          "delete"
                      ],
                      "failureHandling": "textOnlyTransactional",
                      "normalizesLineEndings": true,
                      "changeAnnotationSupport": {
                          "groupsOnLabel": true
                      }
                  },
                  "configuration": """ + supports_configuration.string() + """
,
                  "didChangeWatchedFiles": {
                      "dynamicRegistration": true,
                      "relativePatternSupport": true
                  },
                  "symbol": {
                      "dynamicRegistration": true,
                      "symbolKind": {
                          "valueSet": [
                              1,
                              2,
                              3,
                              4,
                              5,
                              6,
                              7,
                              8,
                              9,
                              10,
                              11,
                              12,
                              13,
                              14,
                              15,
                              16,
                              17,
                              18,
                              19,
                              20,
                              21,
                              22,
                              23,
                              24,
                              25,
                              26
                          ]
                      },
                      "tagSupport": {
                          "valueSet": [
                              1
                          ]
                      },
                      "resolveSupport": {
                          "properties": [
                              "location.range"
                          ]
                      }
                  },
                  "codeLens": {
                      "refreshSupport": true
                  },
                  "executeCommand": {
                      "dynamicRegistration": true
                  },
                  "didChangeConfiguration": {
                      "dynamicRegistration": """ + did_change_configuration_dynamic_registration.string() + """
                  },
                  "workspaceFolders": true,
                  "foldingRange": {
                      "refreshSupport": true
                  },
                  "semanticTokens": {
                      "refreshSupport": true
                  },
                  "fileOperations": {
                      "dynamicRegistration": true,
                      "didCreate": true,
                      "didRename": true,
                      "didDelete": true,
                      "willCreate": true,
                      "willRename": true,
                      "willDelete": true
                  },
                  "inlineValue": {
                      "refreshSupport": true
                  },
                  "inlayHint": {
                      "refreshSupport": true
                  },
                  "diagnostics": {
                      "refreshSupport": true
                  }
              },
              "textDocument": {
                  "publishDiagnostics": {
                      "relatedInformation": true,
                      "versionSupport": false,
                      "tagSupport": {
                          "valueSet": [
                              1,
                              2
                          ]
                      },
                      "codeDescriptionSupport": true,
                      "dataSupport": true
                  },
                  "synchronization": {
                      "dynamicRegistration": true,
                      "willSave": true,
                      "willSaveWaitUntil": true,
                      "didSave": true
                  },
                  "completion": {
                      "dynamicRegistration": true,
                      "contextSupport": true,
                      "completionItem": {
                          "snippetSupport": true,
                          "commitCharactersSupport": true,
                          "documentationFormat": [
                              "markdown",
                              "plaintext"
                          ],
                          "deprecatedSupport": true,
                          "preselectSupport": true,
                          "tagSupport": {
                              "valueSet": [
                                  1
                              ]
                          },
                          "insertReplaceSupport": true,
                          "resolveSupport": {
                              "properties": [
                                  "documentation",
                                  "detail",
                                  "additionalTextEdits"
                              ]
                          },
                          "insertTextModeSupport": {
                              "valueSet": [
                                  1,
                                  2
                              ]
                          },
                          "labelDetailsSupport": true
                      },
                      "insertTextMode": 2,
                      "completionItemKind": {
                          "valueSet": [
                              1,
                              2,
                              3,
                              4,
                              5,
                              6,
                              7,
                              8,
                              9,
                              10,
                              11,
                              12,
                              13,
                              14,
                              15,
                              16,
                              17,
                              18,
                              19,
                              20,
                              21,
                              22,
                              23,
                              24,
                              25
                          ]
                      },
                      "completionList": {
                          "itemDefaults": [
                              "commitCharacters",
                              "editRange",
                              "insertTextFormat",
                              "insertTextMode",
                              "data"
                          ]
                      }
                  },
                  "hover": {
                      "dynamicRegistration": true,
                      "contentFormat": [
                          "markdown",
                          "plaintext"
                      ]
                  },
                  "signatureHelp": {
                      "dynamicRegistration": true,
                      "signatureInformation": {
                          "documentationFormat": [
                              "markdown",
                              "plaintext"
                          ],
                          "parameterInformation": {
                              "labelOffsetSupport": true
                          },
                          "activeParameterSupport": true
                      },
                      "contextSupport": true
                  },
                  "definition": {
                      "dynamicRegistration": true,
                      "linkSupport": true
                  },
                  "references": {
                      "dynamicRegistration": true
                  },
                  "documentHighlight": {
                      "dynamicRegistration": true
                  },
                  "documentSymbol": {
                      "dynamicRegistration": true,
                      "symbolKind": {
                          "valueSet": [
                              1,
                              2,
                              3,
                              4,
                              5,
                              6,
                              7,
                              8,
                              9,
                              10,
                              11,
                              12,
                              13,
                              14,
                              15,
                              16,
                              17,
                              18,
                              19,
                              20,
                              21,
                              22,
                              23,
                              24,
                              25,
                              26
                          ]
                      },
                      "hierarchicalDocumentSymbolSupport": true,
                      "tagSupport": {
                          "valueSet": [
                              1
                          ]
                      },
                      "labelSupport": true
                  },
                  "codeAction": {
                      "dynamicRegistration": true,
                      "isPreferredSupport": true,
                      "disabledSupport": true,
                      "dataSupport": true,
                      "resolveSupport": {
                          "properties": [
                              "edit"
                          ]
                      },
                      "codeActionLiteralSupport": {
                          "codeActionKind": {
                              "valueSet": [
                                  "",
                                  "quickfix",
                                  "refactor",
                                  "refactor.extract",
                                  "refactor.inline",
                                  "refactor.rewrite",
                                  "source",
                                  "source.organizeImports"
                              ]
                          }
                      },
                      "honorsChangeAnnotations": true
                  },
                  "codeLens": {
                      "dynamicRegistration": true
                  },
                  "formatting": {
                      "dynamicRegistration": true
                  },
                  "rangeFormatting": {
                      "dynamicRegistration": true,
                      "rangesSupport": true
                  },
                  "onTypeFormatting": {
                      "dynamicRegistration": true
                  },
                  "rename": {
                      "dynamicRegistration": true,
                      "prepareSupport": true,
                      "prepareSupportDefaultBehavior": 1,
                      "honorsChangeAnnotations": true
                  },
                  "documentLink": {
                      "dynamicRegistration": true,
                      "tooltipSupport": true
                  },
                  "typeDefinition": {
                      "dynamicRegistration": true,
                      "linkSupport": true
                  },
                  "implementation": {
                      "dynamicRegistration": true,
                      "linkSupport": true
                  },
                  "colorProvider": {
                      "dynamicRegistration": true
                  },
                  "foldingRange": {
                      "dynamicRegistration": true,
                      "rangeLimit": 5000,
                      "lineFoldingOnly": true,
                      "foldingRangeKind": {
                          "valueSet": [
                              "comment",
                              "imports",
                              "region"
                          ]
                      },
                      "foldingRange": {
                          "collapsedText": false
                      }
                  },
                  "declaration": {
                      "dynamicRegistration": true,
                      "linkSupport": true
                  },
                  "selectionRange": {
                      "dynamicRegistration": true
                  },
                  "callHierarchy": {
                      "dynamicRegistration": true
                  },
                  "semanticTokens": {
                      "dynamicRegistration": true,
                      "tokenTypes": [
                          "namespace",
                          "type",
                          "class",
                          "enum",
                          "interface",
                          "struct",
                          "typeParameter",
                          "parameter",
                          "variable",
                          "property",
                          "enumMember",
                          "event",
                          "function",
                          "method",
                          "macro",
                          "keyword",
                          "modifier",
                          "comment",
                          "string",
                          "number",
                          "regexp",
                          "operator",
                          "decorator"
                      ],
                      "tokenModifiers": [
                          "declaration",
                          "definition",
                          "readonly",
                          "static",
                          "deprecated",
                          "abstract",
                          "async",
                          "modification",
                          "documentation",
                          "defaultLibrary"
                      ],
                      "formats": [
                          "relative"
                      ],
                      "requests": {
                          "range": true,
                          "full": {
                              "delta": true
                          }
                      },
                      "multilineTokenSupport": false,
                      "overlappingTokenSupport": false,
                      "serverCancelSupport": true,
                      "augmentsSyntaxTokens": true
                  },
                  "linkedEditingRange": {
                      "dynamicRegistration": true
                  },
                  "typeHierarchy": {
                      "dynamicRegistration": true
                  },
                  "inlineValue": {
                      "dynamicRegistration": true
                  },
                  "inlayHint": {
                      "dynamicRegistration": true,
                      "resolveSupport": {
                          "properties": [
                              "tooltip",
                              "textEdits",
                              "label.tooltip",
                              "label.location",
                              "label.command"
                          ]
                      }
                  },
                  "diagnostic": {
                      "dynamicRegistration": true,
                      "relatedDocumentSupport": false
                  }
              },
              "window": {
                  "showMessage": {
                      "messageActionItem": {
                          "additionalPropertiesSupport": true
                      }
                  },
                  "showDocument": {
                      "support": true
                  },
                  "workDoneProgress": true
              },
              "general": {
                  "staleRequestSupport": {
                      "cancel": true,
                      "retryOnContentModified": [
                          "textDocument/semanticTokens/full",
                          "textDocument/semanticTokens/range",
                          "textDocument/semanticTokens/full/delta"
                      ]
                  },
                  "regularExpressions": {
                      "engine": "ECMAScript",
                      "version": "ES2020"
                  },
                  "markdown": {
                      "parser": "marked",
                      "version": "1.1.0"
                  },
                  "positionEncodings": [
                      "utf-16"
                  ]
              },
              "notebookDocument": {
                  "synchronization": {
                      "dynamicRegistration": true,
                      "executionSummarySupport": true
                  }
              }
          },
          "trace": "verbose",
          "workspaceFolders": [
              {
                  "uri": "file://""" + dir + """
",
                  "name": "workspace"
              }
          ]
      }
    }
    """)


class \nodoc\ iso _InitializeTest is UnitTest
  fun name(): String => "initialize"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let harness = TestHarness.create(
      h,
      TestCompiler(h),
      object iso is MessageHandler
        fun handle_request(h: TestHelper, req: RequestMessage val, server: BaseProtocol) =>
          h.assert_eq[String](Methods.initialize(), req.method)
      end,
      {(h: TestHelper, harness: TestHarness ref): Bool =>
        let res = h.assert_eq[USize](1, harness.sent.size())
        h.complete(true)
        res
      }
      where after_sends = 1, after_logs = USize.max_value()
    )
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    harness.send_to_server(
      _LspMsg.initialize(workspace_dir)
    )


class \nodoc\ iso _WorkspaceConfigurationTest is UnitTest
  fun name(): String => Methods.workspace().configuration()

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    // ensure compiler got settings applied
    let compiler = TestCompiler(
      h
      where
        expected_defines = ["FOO"; "BAR"],
        expected_ponypath = ["/pony/path"; "/foo/bar"]
      )
    let harness = TestHarness.create(
      h,
      compiler,
      object iso is MessageHandler
        fun handle_response(h: TestHelper, res: ResponseMessage val, server: BaseProtocol) =>
          try
            h.assert_true(RequestIds.eq(res.id as RequestId , 0))
          end
          // send initialized notification
          server(_LspMsg.initialized())

        fun handle_request(h: TestHelper, req: RequestMessage val, server: BaseProtocol) =>
          // we expect a configuration request here
          h.assert_eq[String](
            Methods.workspace().configuration(),
            req.method
          )
          // send response
          server(
            ResponseMessage(
              req.id,
              JsonArray.push(
                JsonObject
                  .update("defines", JsonArray.push("FOO").push("BAR"))
                  .update("ponypath", JsonArray.push("/pony/path").push("/foo/bar"))
              )
            ).string().iso_array()
          )
      end,
      {(h: TestHelper, harness: TestHarness ref): Bool =>
        true
      }
    )
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let init_msg = _LspMsg.initialize(
      workspace_dir
      where
        did_change_configuration_dynamic_registration = false,
        supports_configuration = true
    )
    harness.send_to_server(consume init_msg)


class \nodoc\ iso _DidChangeConfigurationTest is UnitTest
  fun name(): String =>
    Methods.workspace().did_change_configuration()

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    // ensure compiler got settings applied
    let compiler = TestCompiler(
      h
      where
        expected_defines = ["FOO"; "BAR"],
        expected_ponypath = ["/pony/path"; "/foo/bar"]
      )
    let harness = TestHarness.create(
      h,
      compiler,
      object iso is MessageHandler
        fun handle_response(h: TestHelper, res: ResponseMessage val, server: BaseProtocol) =>
          // this should be the initialize response with server capabilities as result
          try
            h.assert_true(RequestIds.eq(res.id as RequestId, 0))
          end
          // send initialized notification
          server(_LspMsg.initialized())

        fun handle_request(h: TestHelper, req: RequestMessage val, server: BaseProtocol) =>
          // 1.) expect register capability
          h.assert_eq[String](Methods.client().register_capability(), req.method)
          // 2.) send response with null result
          server(ResponseMessage(req.id, None).into_bytes())
          // 3.) send didChangeConfiguration notification
          server(
            Notification(
              Methods.workspace().did_change_configuration(),
              JsonObject
                .update("settings", JsonObject
                  .update("pony-lsp", JsonObject
                    .update("defines", JsonArray.push("FOO").push("BAR"))
                    .update("ponypath", JsonArray.push("/pony/path").push("/foo/bar"))
                  )
                )
            ).into_bytes()
          )
          None
      end,
      {(h: TestHelper, harness: TestHarness ref): Bool =>
        true
      }
    )

    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    harness.send_to_server(
      _LspMsg.initialize(
        workspace_dir
        where
          did_change_configuration_dynamic_registration = true,
          supports_configuration = false
      )
    )

actor TestCompiler is LspCompiler
  let _h: TestHelper
  let _expected_defines: Array[String] val
  let _expected_ponypath: Array[String] val
  new create(
    h: TestHelper,
    expected_defines: Array[String] val = [],
    expected_ponypath: Array[String] val = []
  ) =>
    _h = h
    _expected_defines = expected_defines
    _expected_ponypath = expected_ponypath

  be apply_settings(settings: Settings) =>
    _h.assert_array_eq[String](
      _expected_defines,
      settings.defines()
    )
    _h.assert_array_eq[String](
      _expected_ponypath,
      settings.ponypath()
    )
    _h.complete(true)

  be compile(package: FilePath, paths: Array[String val] val, notify: CompilerNotify tag) =>
    """Most efficient compiler ever"""
    None

interface MessageHandler
  fun ref handle_message(h: TestHelper, m: Message val, server: BaseProtocol) =>
    match m
    | let req: RequestMessage val =>
      this.handle_request(h, req, server)
    | let res: ResponseMessage val =>
      this.handle_response(h, res, server)
    | let n: Notification val =>
      this.handle_notification(h, n, server)
    end

  fun ref handle_request(h: TestHelper, req: RequestMessage, server: BaseProtocol) => 
    None

  fun ref handle_response(h: TestHelper, req: ResponseMessage, server: BaseProtocol) => 
    None

  fun ref handle_notification(h: TestHelper, notification: Notification, server: BaseProtocol) => 
    None

actor TestHarness is Channel
  let sent: Array[Message val] ref = sent.create(8)
  let logs: Array[(String, MessageType)] ref = logs.create(8)
  let h: TestHelper
  let _handler: MessageHandler ref
  let _server: BaseProtocol

  let _after_sends: USize
  let _after_logs: USize
  let _expect_fun: {(TestHelper, TestHarness ref): Bool} val

  new create(
    h': TestHelper,
    compiler: LspCompiler,
    message_handler: MessageHandler iso,
    expect_fun: {(TestHelper, TestHarness ref): Bool} val,
    after_sends: USize = 1,
    after_logs: USize = 1)
  =>
    this.h = h'
    this._handler = recover ref consume message_handler end
    this._server = BaseProtocol(
      LanguageServer(this, h.env, compiler)
    )
    this._expect_fun = expect_fun
    this._after_sends = after_sends
    this._after_logs = after_logs

  be send_to_server(msg: Array[U8] iso) =>
    this._server(consume msg)

  be send(msg: Message val) =>
    """here we basically receive from the server"""
    sent.push(msg)
    // make it possible to directly respond to messages from the server
    this._handler.handle_message(this.h, msg, this._server)
    do_expect()

  be log(data: String val, message_type: MessageType = Debug) =>
    h.log("LOG: " + data)
    logs.push((data, message_type))
    do_expect()

  fun ref do_expect(force: Bool = false) =>
    if force or (sent.size() >= this._after_sends) or (logs.size() >= this._after_logs) then
      if not this._expect_fun(h, this) then
        // print out some debug stuff
        h.log("SENT")
        for s in sent.values() do
          h.log(s.string())
        end

        h.log("LOGS")
        for (msg, level) in logs.values() do
          h.log("[" + level.string() + "] " + msg)
        end
      end
    end

  be set_notifier(notifier: Notifier tag) =>
    None

  be dispose() =>
    h.log("CHANNEL dispose")
    do_expect(where force = true)


