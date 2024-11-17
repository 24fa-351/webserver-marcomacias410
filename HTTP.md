# DEFINITION

HTTP-message   = start-line CRLF
                   *( field-line CRLF )   -- 0...inf
                   CRLF
                   [ message-body ]   -- zero or 1 == optional
                   END

# SAMPLE MESSAGE

start-line CRLF field-line CRLF CRLF END


message-body: read("content-length") bytes
