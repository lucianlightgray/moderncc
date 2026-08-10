function pad(s,   r) {
	r = s;
	while (length(r) < 8)
		r = "0" r;
	return r;
}
/^[0-9a-f]+ <.*>:$/ {
	fn = $2;
	sub(/^</, "", fn);
	sub(/>:$/, "", fn);
	next;
}
fn == "" { next }
/R_[A-Z0-9_]+/ {
	off = $1;
	sub(/:$/, "", off);
	sym = $3;
	sub(/[-+]0x[0-9a-f]+$/, "", sym);
	print "REL", fn, pad(off), sym;
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
	tgt = w[nf - 1];
	if (tgt !~ /^[0-9a-f]+$/)
		next;
	if (pad(tgt) < pad(off))
		print "LOOP", fn, pad(tgt), pad(off);
}
