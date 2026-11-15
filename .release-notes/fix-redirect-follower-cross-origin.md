## Fix http_client crashing or misrouting on cross-origin redirects

Cross-origin HTTP redirects through `RedirectFollower` crashed over HTTPS and silently sent the follow-up request on the wrong connection over HTTP. The old connection is now closed immediately before the new one is created, so late events from the old socket cannot reach the new connection.
