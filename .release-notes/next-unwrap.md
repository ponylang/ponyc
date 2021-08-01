## Make working with Promise[Promise[B]] chains easier

We've added a new method `flatten_next` to `Promise` in order to make working with promises of promises easier.

`flatten_next` is a companion to `next`. It operates in an identical fashion except for the type of the fulfilled promise. Whereas `next` returns a type `B`, `flatten_next` returns `Promise[B]`.

Why is `flatten_next` valuable given that next could take a `B` that is of a type like `Promise[String]`? Let's start with some code to demonstrate the problem that arises when returning `Promise[Promise[B]]` from `next`.

Let's say we have a library for accessing the GitHub REST API:

```pony
class GitHub
  new create(personal_access_token: String)

  fun get_repo(repo: String): Promise[Repository]

class Repo
  fun get_issue(number: I64): Promise[Issue]

class Issue
  fun title(): String
```

And we want to use this promise based API to look up the title of an issue. Without `flatten_next`, we could attempt to do the following using `next`:

```pony
actor Main
  new create(env: Env) =>
    let repo: Promise[Repo] =
      GitHub("my token").get_repo("ponylang/ponyc")

    //
    // do something with the repo once the promise is fulfilled
    // in our case, get the issue
    //

    let issue = Promise[Promise[Issue]] =
      repo.next[Promise[Issue]](FetchIssue~apply(1))

    // once we get the issue, print the title
    issue.next[None](PrintIssueTitle~apply(env.out))

primitive FetchIssue
  fun apply(number: I64, repo: Repo): Promise[Issue] =>
    repo.get_issue(number)

primitive PrintIssueTitle
  fun apply(out: OutStream, issue: Promise[Issue]) =>
    // O NO! We can't print the title
    // We don't have an issue, we have a promise for an issue
```

Take a look at what happens in the example, when we get to `PrintIssueTitle`, we can't print anything because we "don't have anything". In order to print the issue title, we need an `Issue` not a `Promise[Issue]`.

We could solve this by doing something like this:

```pony
primitive PrintIssueTitle
  fun apply(out: OutStream, issue: Promise[Issue]) =>
    issue.next[None](ActuallyPrintIssueTitle~apply(out))

primitive ActuallyPrintIssueTitle
  fun apply(out: OutStream, issue: Issue) =>
    out.print(issue.title())
```

That will work, however, it is kind of awful. When looking at:

```pony
    let repo: Promise[Repo] =
      GitHub("my token").get_repo("ponylang/ponyc")
    let issue = Promise[Promise[Issue]] =
      repo.next[Promise[Issue]](FetchIssue~apply(1))
    issue.next[None](PrintIssueTitle~apply(env.out))
```

it can be hard to follow what is going on. We can only tell what is happening because we gave `PrintIssueTitle` a very misleading name; it doesn't print an issue title.

`flatten_next` addresses the problem of "we want the `Issue`, not the intermediate `Promise`". `flatten_next` takes an intermediate promise and unwraps it into the fulfilled type. You get to write your promise chain without having to worry about intermediate promises.

Updated to use `flatten_next`, our API example becomes:

```pony
actor Main
  new create(env: Env) =>
    let repo: Promise[Repo] =
      GitHub("my token").get_repo("ponylang/ponyc")

    let issue = Promise[Issue] =
      repo.flatten_next[Issue](FetchIssue~apply(1))

    issue.next[None](PrintIssueTitle~apply(env.out))

primitive FetchIssue
  fun apply(number: I64, repo: Repo): Promise[Issue] =>
    repo.get_issue(number)

primitive PrintIssueTitle
  fun apply(out: OutStream, issue: Issue) =>
    out.print(issue.title())
```

Our promise `Issue`, is no longer a `Promise[Promise[Issue]]`. By using `flatten_next`, we have a much more manageable `Promise[Issue]` instead.

Other than unwrapping promises for you, `flatten_next` otherwise acts the same as `next` so all the same rules apply to fulfillment and rejection.
