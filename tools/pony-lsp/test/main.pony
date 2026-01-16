use "pony_test"
use "buffered"
use "files"
use ast = "ast"
use ".."

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_InitializeTest)
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

class \nodoc\ iso _InitializeTest is UnitTest
  fun name(): String => "initialize"

  fun apply(h: TestHelper) =>
    h.long_test(10_000_000_000)

    let channel = TestChannel.create(
      h,
      {(h: TestHelper, channel: TestChannel ref): Bool =>
        let res = h.assert_eq[USize](1, channel.sent.size())
        h.complete(true)
        res
      }
      where after_sends = 1, after_logs = USize.max_value()
    )
    let pony_path =
      match ast.PonyPath(h.env)
      | let p: String => p
      | None => ""
      end

    let server = LanguageServer(channel, h.env, pony_path)

    let base = BaseProtocol(server)
    base(_LspMsg("""
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
          "rootPath": "/home/mat/dev/pony/valbytes",
          "rootUri": "file:///home/mat/dev/pony/valbytes",
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
                  "configuration": true,
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
                      "dynamicRegistration": true
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
                  "uri": "file:///home/mat/dev/pony/valbytes",
                  "name": "valbytes"
              }
          ]
      }
    }
    """))

actor TestChannel is Channel
  let sent: Array[Message val] ref = sent.create(8)
  let logs: Array[(String, MessageType)] ref = logs.create(8)
  let h: TestHelper

  let _after_sends: USize
  let _after_logs: USize
  let _expect_fun: {(TestHelper, TestChannel ref): Bool} val

  new create(
    h': TestHelper,
    expect_fun: {(TestHelper, TestChannel ref): Bool} val,
    after_sends: USize = 1,
    after_logs: USize = 1)
  =>
    this.h = h'
    this._expect_fun = expect_fun
    this._after_sends = after_sends
    this._after_logs = after_logs

  be send(msg: Message val) =>
    h.log(msg.string())
    sent.push(msg)
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
