#!/usr/bin/env bash
# Delete tags from a local repo

usage() {
	echo "Delete tags from local  repo"
	echo "  Usage: $0 regex=yyy"
	echo "    --regex=yyy yyy is the regular expression to match tags"
	echo "                yyy maybe \"*\", \"all\" or empty to match all tags"
	echo "    --help      Displays this help message"
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
#echo "regex=${regex}"

if [[ "${regex}" == "" || "${dsp_usage}" == "true" ]]; then
	usage
       	exit 1
fi


if [[ "${regex}" = "all" || "${regex}" == "*" ]]; then
	regex=""
fi

IFS=$'\n' tags=( $(git show-ref --tags) )
tags_to_del=()
#echo "tags: ${tags[@]}"
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
			#echo "tags_to_del+=${tag}"
			tags_to_del+=(${tag})
		fi
	fi
done

# Capture the output into an array
if [[ ${#tags_to_del[@]} > 0 ]]; then
	line="${tags_to_del[@]}"
	echo "${line}"
	read -p "Delete? (y/N): " response
	if [[ "${response}" = "Y" || "${response}" = "y" ]]; then
		git tag -d ${tags_to_del[@]}
	fi
else
	echo "No matching tags"
fi
