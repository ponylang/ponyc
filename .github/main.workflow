workflow "delete-branch-on-merge" {
  on = "pull_request"
  resolves = ["SvanBoxel/delete-merged-branch"]
}

action "SvanBoxel/delete-merged-branch" {
  uses = "SvanBoxel/delete-merged-branch@v1.3.3"
  secrets = ["GITHUB_TOKEN"]
}
