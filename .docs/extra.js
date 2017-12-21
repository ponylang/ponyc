// https://github.com/mkdocs/mkdocs/issues/803
$(document).ready(() => $('.wy-nav-side').scrollTop($('li.toctree-l1.current').offset().top - $('.wy-nav-side').offset().top - 80));

try {
    var preElem = document.getElementsByClassName("pony-full-source")[0].nextElementSibling;
    var codeElem = preElem.children[0];
    var lines = $(codeElem).text().split('\n').length - 1;
    var numbering = $('<code class="code-line-numbers"></code>').addClass('pre-numbering');
    $(codeElem)
        .addClass('has-numbering')
        .parent()
        .append(numbering);

    for (i = 1; i <= lines; i++) {
        numbering.append($('<div></div>').text(i));
    }
} catch (e) {}

try {
    var line_number = window.location.href.substring(window.location.href.indexOf("#") + 1);
    var line_numbers = document.getElementsByClassName("code-line-numbers")[0];
    var elem = line_numbers.children[line_number - 1];
    elem.style.backgroundColor = "yellow";
    elem.scrollIntoView();
} catch (e) {}