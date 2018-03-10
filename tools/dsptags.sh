#!/usr/bin/env bash
# display tags from a git repo at a url
# with an optional regex to match against

usage() {
	echo "Display tags from current or remote repo"
	echo "  Usage: $0 {url=xxx} {regex=yyy}"
	echo "    --url=xxx   xxx is the url of the remote repo or empty local"
	echo "    --regex=yyy yyy is the regular expression to match tags"
	echo "                yyy maybe \"*\", \"all\" or empty to match all tags"
	echo "    --help      Displays this help message"
	echo
	echo "  A parameter in braces is optional"
	exit 1
}

IFS=' ' read -r -a params <<< "${@}"
#echo "params: ${params[@]}"

dsp_usage=""
url=""
regex=""

for name_value in "${params[@]}"; do
	#echo "name_value: ${name_value}"
	IFS="=" read -r name value <<< "${name_value}"
	#echo "name=${name} value=${value}"
	case "${name}" in
		/?|-?|?|-h|--help|help)
			#echo "help value='${value}'"
			dsp_usage="true"
			;;
		--url|url)
			#echo "url='${value}'"
			url=${value}
			;;
		--regex|regex)
			#echo "regex='${value}'"
			regex=${value}
			;;
		*)
			echo "unknown: name=${name} value='${value}'"
			dsp_usage="true"
			;;
	esac
done
#echo "url=${url} regex=${regex}"

if [[ "${dsp_usage}" == "true" ]]; then
	usage
       	exit 1
fi

# Read the output of git ls-remote with one line per array
if [[ "${url}" == "" ]]; then
	IFS=$'\n' tags=( $(git show-ref --tags) )
else
	IFS=$'\n' tags=( $(git ls-remote --tags ${url}) )
fi
#echo "tags len=${#tags[@]}"
#echo "tags ${tags[@]}"

if [[ "${regex}" = "all" || "${regex}" == "*" ]]; then
	regex=""
fi

for element in "${tags[@]}"; do
	#echo "${element}"
	line=( $(echo "${element}" | sed -e "s/[[:space:]]\+/ /g") )
	#echo "line=${line}"
	IFS=' ' read -r sha1 ref <<< "${line}"
	#echo "sha1=${sha1} ref=${ref}"
	IFS='/' read -r _ _ tag <<< "${ref}"
	#echo "tag=${tag}"

	# Test that the tag is NOT empty and does NOT end in "^{}"
	if [[ "${tag}" != "" && "${tag}" != *"^{}" ]]; then
		#echo "tag=${tag} regex=${regex}"
		if [[ "${regex}" == "" || ${tag} =~ ${regex} ]]; then
			# Display
			echo "${tag} (at ${sha1})"
		fi
	fi
done
