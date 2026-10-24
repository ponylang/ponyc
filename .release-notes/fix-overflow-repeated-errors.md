## Fix repeated overflow errors for long numeric literals

A numeric literal that exceeded the maximum integer size produced many duplicate "overflow in numeric literal" errors instead of one. A 2,000-digit literal gave 52 errors and 108 KB of stderr output; now it gives one.
