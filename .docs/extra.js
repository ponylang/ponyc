// https://github.com/mkdocs/mkdocs/issues/803
$(document).ready(() => $('.wy-nav-side').scrollTop($('li.toctree-l1.current').offset().top - $('.wy-nav-side').offset().top - 80));
