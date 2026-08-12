function pad(s,   r) {
	r = s;
	sub(/^0+/, "", r);
	if (r == "")
		r = "0";
	while (length(r) < 8)
		r = "0" r;
	return r;
}
function hex(s,   i, c, v, d) {
	v = 0;
	sub(/^0[xX]/, "", s);
	for (i = 1; i <= length(s); i++) {
		c = tolower(substr(s, i, 1));
		d = index("0123456789abcdef", c) - 1;
		if (d < 0)
			return -1;
		v = v * 16 + d;
	}
	return v;
}
function unmangle(s) {
	sub(/^_/, "", s);
	return s;
}
/^[0-9a-f]+ <.*>:$/ {
	fn = $2;
	sub(/^</, "", fn);
	sub(/>:$/, "", fn);
	fn = unmangle(fn);
	next;
}
fn == "" { next }
/R_[A-Z0-9_]+/ {
	off = $1;
	sub(/:$/, "", off);
	sym = $3;
	sub(/[-+]0x[0-9a-f]+$/, "", sym);
	sym = unmangle(sym);
	if (!(fn == prevfn && sym == prevsym && hex(off) == prevoff + 4))
		print "REL", fn, pad(off), sym;
	prevfn = fn;
	prevsym = sym;
	prevoff = hex(off);
	next;
}
{
	line = $0;
	sub(/#.*$/, "", line);
	sub(/^[ \t]+/, "", line);
	sub(/[ \t]+$/, "", line);
	nf = split(line, w, /[ \t]+/);
	if (nf < 2)
		next;
	if (w[1] !~ /^[0-9a-f]+:$/)
		next;
	off = w[1];
	sub(/:$/, "", off);
	if (w[nf] !~ /^<.*>$/)
		next;
	if (nf < 3)
		next;
	mn = w[nf - 2];
	if (mn !~ /^(b|b\..+|cbz|cbnz|tbz|tbnz|j[a-z]+|loop[a-z]*)$/)
		next;
	tgt = w[nf - 1];
	sub(/^0x/, "", tgt);
	if (tgt !~ /^[0-9a-f]+$/)
		next;
	if (pad(tgt) < pad(off))
		print "LOOP", fn, pad(tgt), pad(off);
}
