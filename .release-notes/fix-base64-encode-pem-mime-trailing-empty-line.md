## Fix Base64.encode_pem and Base64.encode_mime producing trailing empty lines

`Base64.encode_pem` and `Base64.encode_mime` appended an extra CRLF when the input was empty or when its encoding filled the last line exactly (multiples of 48 bytes for PEM, 57 for MIME). Empty input produced `"\r\n"` instead of `""`, and exact-line inputs ended with `"\r\n\r\n"` instead of `"\r\n"`.
