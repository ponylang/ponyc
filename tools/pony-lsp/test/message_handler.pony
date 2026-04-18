use ".."
use "pony_test"
use "pony_compiler"
use "files"

actor TestCompiler is LspCompiler
  """
  Test compiler that validates settings.
  """
  let _h: TestHelper
  let _expected_defines: Array[String] val
  let _expected_ponypath: Array[String] val

  new create(
    h: TestHelper,
    expected_defines: Array[String] val = [],
    expected_ponypath: Array[String] val = [])
  =>
    _h = h
    _expected_defines = expected_defines
    _expected_ponypath = expected_ponypath

  be apply_settings(
    settings: (Settings | None))
  =>
    match settings
    | let settings': Settings =>
      var all_good =
        _h.assert_array_eq[String](
          _expected_defines,
          settings'.defines()
        )
      all_good =
        _h.assert_array_eq[String](
          _expected_ponypath,
          settings'.ponypath()
        ) and all_good
      _h.complete(all_good)
    end

  be compile(
    package: FilePath,
    paths: Array[String val] val,
    notify: CompilerNotify tag,
    notify_passes: Array[PassId] val)
  =>
    """
    Most efficient compiler ever.
    """
    None

interface MessageHandler
  """
  Handler for messages received by the test harness.
  """

  fun ref handle_message(
    h: TestHelper,
    m: Message val,
    server: BaseProtocol)
  =>
    match m
    | let req: RequestMessage val =>
      this.handle_request(h, req, server)
    | let res: ResponseMessage val =>
      this.handle_response(h, res, server)
    | let n: Notification val =>
      this.handle_notification(h, n, server)
    else
      h.log(
        "Unexpected message kind: " + m.string())
    end

  fun ref handle_request(
    h: TestHelper,
    req: RequestMessage,
    server: BaseProtocol)
  =>
    None

  fun ref handle_response(
    h: TestHelper,
    req: ResponseMessage,
    server: BaseProtocol)
  =>
    None

  fun ref handle_notification(
    h: TestHelper,
    notification: Notification,
    server: BaseProtocol)
  =>
    None

actor TestHarness is Channel
  """
  Test harness that acts as a communication channel and intercepts messages.
  """
  let sent: Array[Message val] ref =
    sent.create(8)
  let logs: Array[(String, MessageType)] ref =
    logs.create(8)
  let h: TestHelper
  let _handler: MessageHandler ref
  let _server: BaseProtocol
  let _after_sends: USize
  let _after_logs: USize
  let _expect_fun:
    {(TestHelper, TestHarness ref): Bool} val

  new create(
    h': TestHelper,
    compiler: LspCompiler,
    message_handler: MessageHandler iso,
    expect_fun:
      {(TestHelper, TestHarness ref): Bool} val,
    after_sends: USize = 1,
    after_logs: USize = 1)
  =>
    this.h = h'
    this._handler =
      recover ref
        consume message_handler
      end
    this._server =
      BaseProtocol(
        LanguageServer(this, h.env, compiler)
      )
    this._expect_fun = expect_fun
    this._after_sends = after_sends
    this._after_logs = after_logs

  be send_to_server(msg: Array[U8] iso) =>
    this._server(consume msg)

  be send(msg: Message val) =>
    """
    Receive a message from the server.
    """
    sent.push(msg)
    // make it possible to directly respond to
    // messages from the server
    this._handler.handle_message(
      this.h, msg, this._server)
    do_expect()

  be log(
    data: String val,
    message_type: MessageType = Debug)
  =>
    h.log("LOG: " + data)
    logs.push((data, message_type))
    do_expect()

  fun ref do_expect(force: Bool = false) =>
    if force
      or (sent.size() >= this._after_sends)
      or (logs.size() >= this._after_logs)
    then
      if not this._expect_fun(h, this) then
        // print out some debug stuff
        h.log("SENT")
        for s in sent.values() do
          h.log(s.string())
        end

        h.log("LOGS")
        for (msg, level) in logs.values() do
          h.log(
            "[" + level.string() + "] " + msg)
        end
      end
    end

  be set_notifier(notifier: Notifier tag) =>
    None

  be dispose() =>
    h.log("CHANNEL dispose")
    do_expect(where force = true)

primitive LspMsg
  """
  Utility for constructing LSP messages in tests.
  """

  fun tag apply(msg: String val):
    Array[U8] iso^
  =>
    let len = msg.size()
    let s =
      recover iso
        String.create(len + 25)
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

  fun tag _json_escape(
    s: String val): String val
  =>
    """
    Escape backslashes for safe embedding in JSON
    string literals.
    """
    ifdef windows then
      if s.contains("\\") then
        let out = s.clone()
        out.replace("\\", "\\\\")
        consume out
      else
        s
      end
    else
      s
    end

  fun tag initialize(
    dir: String,
    did_change_configuration_dynamic_registration:
      Bool = true,
    supports_configuration: Bool = true
  ): Array[U8] iso^ =>
    let safe_dir = _json_escape(dir)
    let dir_uri = Uris.from_path(dir)
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
          "rootPath": """" + safe_dir + """
",
          "rootUri": """" + dir_uri + """
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
                              1,2,3,4,5,6,7,8,9,10,
                              11,12,13,14,15,16,17,18,19,20,
                              21,22,23,24,25,26
                          ]
                      },
                      "tagSupport": {
                          "valueSet": [1]
                      },
                      "resolveSupport": {
                          "properties": ["location.range"]
                      }
                  },
                  "codeLens": {"refreshSupport": true},
                  "executeCommand": {"dynamicRegistration": true},
                  "didChangeConfiguration": {
                      "dynamicRegistration": """ +
      did_change_configuration_dynamic_registration
        .string() + """
                  },
                  "workspaceFolders": true,
                  "foldingRange": {"refreshSupport": true},
                  "semanticTokens": {"refreshSupport": true},
                  "fileOperations": {
                      "dynamicRegistration": true,
                      "didCreate": true,
                      "didRename": true,
                      "didDelete": true,
                      "willCreate": true,
                      "willRename": true,
                      "willDelete": true
                  },
                  "inlineValue": {"refreshSupport": true},
                  "inlayHint": {"refreshSupport": true},
                  "diagnostics": {"refreshSupport": true}
              },
              "textDocument": {
                  "publishDiagnostics": {
                      "relatedInformation": true,
                      "versionSupport": false,
                      "tagSupport": {"valueSet": [1,2]},
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
                          "documentationFormat": ["markdown","plaintext"],
                          "deprecatedSupport": true,
                          "preselectSupport": true,
                          "tagSupport": {"valueSet": [1]},
                          "insertReplaceSupport": true,
                          "resolveSupport": {
                              "properties": [
                                  "documentation",
                                  "detail",
                                  "additionalTextEdits"
                              ]
                          },
                          "insertTextModeSupport": {"valueSet": [1,2]},
                          "labelDetailsSupport": true
                      },
                      "insertTextMode": 2,
                      "completionItemKind": {
                          "valueSet": [
                              1,2,3,4,5,6,7,8,9,
                              10,11,12,13,14,15,
                              16,17,18,19,20,21,
                              22,23,24,25
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
                          "documentationFormat": ["markdown","plaintext"],
                          "parameterInformation": {"labelOffsetSupport": true},
                          "activeParameterSupport": true
                      },
                      "contextSupport": true
                  },
                  "definition": {
                      "dynamicRegistration": true,
                      "linkSupport": true
                  },
                  "references": {"dynamicRegistration": true},
                  "documentHighlight": {"dynamicRegistration": true},
                  "documentSymbol": {
                      "dynamicRegistration": true,
                      "symbolKind": {
                          "valueSet": [
                              1,2,3,4,5,6,7,8,9,
                              10,11,12,13,14,15,
                              16,17,18,19,20,21,
                              22,23,24,25,26
                          ]
                      },
                      "hierarchicalDocumentSymbolSupport": true,
                      "tagSupport": {"valueSet": [1]},
                      "labelSupport": true
                  },
                  "codeAction": {
                      "dynamicRegistration": true,
                      "isPreferredSupport": true,
                      "disabledSupport": true,
                      "dataSupport": true,
                      "resolveSupport": {"properties": ["edit"]},
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
                  "codeLens": {"dynamicRegistration": true},
                  "formatting": {"dynamicRegistration": true},
                  "rangeFormatting": {
                      "dynamicRegistration": true,
                      "rangesSupport": true
                  },
                  "onTypeFormatting": {"dynamicRegistration": true},
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
                  "colorProvider": {"dynamicRegistration": true},
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
                  "selectionRange": {"dynamicRegistration": true},
                  "callHierarchy": {"dynamicRegistration": true},
                  "semanticTokens": {
                      "dynamicRegistration": true,
                      "tokenTypes": [
                          "namespace","type",
                          "class","enum",
                          "interface","struct",
                          "typeParameter",
                          "parameter",
                          "variable","property",
                          "enumMember","event",
                          "function","method",
                          "macro","keyword",
                          "modifier","comment",
                          "string","number",
                          "regexp","operator",
                          "decorator"
                      ],
                      "tokenModifiers": [
                          "declaration",
                          "definition",
                          "readonly","static",
                          "deprecated",
                          "abstract","async",
                          "modification",
                          "documentation",
                          "defaultLibrary"
                      ],
                      "formats": ["relative"],
                      "requests": {
                          "range": true,
                          "full": {"delta": true}
                      },
                      "multilineTokenSupport": false,
                      "overlappingTokenSupport": false,
                      "serverCancelSupport": true,
                      "augmentsSyntaxTokens": true
                  },
                  "linkedEditingRange": {"dynamicRegistration": true},
                  "typeHierarchy": {"dynamicRegistration": true},
                  "inlineValue": {"dynamicRegistration": true},
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
                  "showDocument": {"support": true},
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
                  "positionEncodings": ["utf-16"]
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
                  "uri": """" + dir_uri + """
",
                  "name": "workspace"
              }
          ]
      }
    }
    """)
