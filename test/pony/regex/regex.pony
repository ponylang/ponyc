use "regex"
use "collections"

actor Main
  new create(env: Env) =>
    try
      let pattern = env.args(1)
      let data = env.args(2)

      with regex = Regex(pattern) do
        env.out.print(regex(data).string())

        for i in Range(0, regex.count()) do
          env.out.print(regex.index(i))
        end

        try
          env.out.print(regex.find("test"))
        end

        env.out.print(regex(data) = env.args(3))
      end
    end
