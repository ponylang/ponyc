use "regex"
use "collections"

actor Main
  new create(env: Env) =>
    try
      let pattern = env.args(1)
      let data = env.args(2)
      let regex = Regex(pattern)

      env.out.print((regex == data).string())

      try
        let r = regex(data)

        for i in Range(0, r.size()) do
          env.out.print(r(i))
        end

        try
          env.out.print(r.find("test"))
        end
      end

      env.out.print(regex.replace(data, env.args(3)))
    end
