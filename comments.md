# Ideas
fld// add optional additional index for field range
`fld//,/1[/5]`

# known bugs

s// with $Rn
`echo a b c | led r/ flm//1 r//1 rr/ flm//2 r//2 rr/ flm//3 r//3 's//$R3 $R2 $R1'`
teturns `cba` should be `c b a`