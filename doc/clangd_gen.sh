#!/bin/bash
# Generate a JSON file compatible with clangd for indexing. Parses
# the output of the make process, extract gcc lines, and convert to JSON
# using (gnu) AWK
# Join lines that finish by \ -- then split the large command lines around
# && and ||, then look for gcc, then split and look for a .c argument.
# when found, populate the compile_commands.json file
gawk -v dir="$(pwd)" '
BEGIN {
	acc="";line=0;
	print "[";
}
END {
	print "]";
}
/\\ *$/ {
	$0 = gensub(/\\ *$/, "", "g", $0);
	acc = acc $0
	next;
}
/\|\| *$/ {
	acc = acc $0
	next;
}
/^g?make\[([0-9]+)]/ {
	if ($2 != "Entering")
		next;
	dir = gensub(/'\''/, "", "g", $4)
	next;
}
{
	acc = acc $0
	acc = gensub(/([^\\])"/, "\\1\\\\\"", "g", acc);
	acc = gensub(/  +/, " ", "g", acc);
	cnt = split(acc, e, / *\|\| *| *&& */);
	acc = "";
	for (cmd in e) {
		gsub(/[ \t]+/, " ", e[cmd]);
		split(e[cmd], arg, / +/);
		if (!match(arg[1], /g?cc$/) && !match(arg[1], /clang$/))
			continue;
		c_file=""
		for (ai in arg) {
			gsub(/^[ \t]+|[ \t]+$/, "", arg[ai]);
			if (match(arg[ai], /\.c$/)) {
				c_file = arg[ai];
				break;
			}
		}
		if (c_file != "") {
			if (line > 0) printf ",";
			line++;
			printf "{\"directory\":\"%s\",\"file\":\"%s\",\"command\":\"%s\"}\n",
					dir, c_file, e[cmd];
		}
	}
}
'
