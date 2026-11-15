## Fix Format AlignCenter with a multibyte fill character dropping right-hand padding

`Format.apply` with `AlignCenter` and a fill character that takes more than one byte in UTF-8 produced too little padding. The left fill was correct, but the right fill was missing or short. Padding is now symmetric.
