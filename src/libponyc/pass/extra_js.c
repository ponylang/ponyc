char* extra_js_content = " \
try\n \
{\n \
    var originalSourceCode = document.getElementsByClassName(\"pony-full-source\")[0];\n \
    var preElem = document.createElement(\"pre\");\n \
    var codeElem = document.createElement(\"code\");\n \
    codeElem.className = 'pony';\n \
\n \
    preElem.append(codeElem);\n \
    originalSourceCode.parentNode.append(preElem);\n \
    codeElem.innerHTML = originalSourceCode.innerHTML;\n \
    originalSourceCode.parentElement.removeChild(originalSourceCode);\n \
\n \
    var lines = $(codeElem).text().split('\\n').length - 1;      \n \
    var numbering = $('<code class=\"code-line-numbers\"></code>').addClass('pre-numbering');\n \
\n \
    $(codeElem)\n \
        .addClass('has-numbering')\n \
        .parent()\n \
        .append(numbering);\n \
\n \
    for (i = 1; i <= lines; i++) {\n \
        numbering.append($('<div></div>').text(i));\n \
    }\n \
\n \
    hljs.highlightBlock(codeElem);\n \
}\n \
catch (e) \n \
{\n \
\n \
}\n \
\n \
try\n \
{\n \
    var line_number = window.location.href.substring(window.location.href .indexOf(\"#\")+1);\n \
    var line_numbers = document.getElementsByClassName(\"code-line-numbers\")[0];\n \
    var elem = line_numbers.children[line_number - 1];\n \
    elem.style.backgroundColor = \"yellow\";\n \
}\n \
catch (e) \n \
{\n \
\n \
}";