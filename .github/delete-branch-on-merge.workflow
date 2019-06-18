workflow "Delete Branch on Merge" {
  on = "pull_request"
  resolves = ["SvanBoxel/delete-merged-branch"]
}

action "SvanBoxel/delete-merged-branch" {
  uses = "SvanBoxel/delete-merged-branch@be6c5ba3c5d9e19c04e60b28b7d5309ac3fd12fc"
  secrets = ["GITHUB_TOKEN"]
}
