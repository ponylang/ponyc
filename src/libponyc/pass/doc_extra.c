const char* get_doc_extra_css_content()
{
    return

" \
pre { \n \
    position: relative; \n \
    margin-bottom: 24px; \n \
    border-radius: 3px; \n \
    border: 1px solid #C3CCD0; \n \
    background: #FFF; \n \
    overflow: hidden; \n \
} \n \
    \n \
code { \n \
    display: block; \n \
    padding: 12px 24px; \n \
    overflow-y: auto; \n \
    font-weight: 300; \n \
    font-family: Menlo, monospace; \n \
} \n \
    \n \
code.has-numbering { \n \
    margin-left: 21px; \n \
} \n \
    \n \
.pre-numbering { \n \
    width: 22px;\n \
    position: absolute; \n \
    top: 0; \n \
    left: 0; \n \
    padding: 12px 2px 18px 0; \n \
    border-right: 1px solid #C3CCD0; \n \
    border-radius: 3px 0 0 3px; \n \
    background-color: #EEE; \n \
    text-align: right; \n \
    font-family: Menlo, monospace; \n \
    color: #AAA; \n \
} \n \
\n \
.wy-nav-content { \n \
    max-width: 1200px; \n \
} \n \
";
}

const char* get_doc_extra_js_content() 
{
    return
    
" \
try\n \
{\n \
    var preElem = document.getElementsByClassName(\"pony-full-source\")[0].nextElementSibling;\n \
    var codeElem = preElem.children[0];\n \
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
}