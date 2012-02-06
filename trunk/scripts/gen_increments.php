<?php
	/**
	 * this script generates an increment LUT in C array form. execute
	 * with no arguments for usage. no error verification on arguments
	 * yet.
	 *
	 * author:	Philippe Proulx <eepp.ca>
	 * date:	2012/02/05
	 */
	function print_usage() {
		global $argv;
		
		printf("usage: php %s <sr> <root> <nb>\n" .
			"  <sr>    sampling rate (Hz)\n" .
			"  <root>  root frequency (Hz)\n" .
			"  <nb>    LUT size (number of notes)\n", $argv[0]);
	}
	
	function c_array($type, $name, $values, $sep) {
		$ret = "$type $name [] = {";
		$cnt = count($values);
		foreach ($values as $k => $v) {
			if ($k % $sep == 0 && $k < ($cnt - 1)) {
				$ret .= "\n\t";
			}
			$ret .= $v;
			if ($k < ($cnt - 1)) {
				$ret .= ", ";
			}
		}
		$ret .= "\n};\n";
		
		return $ret;
	}
	
	if ($argc != 4) {
		print_usage();
		
		exit(1);
	}
	
	$sr = $argv[1];
	$sz = 65536;
	$root = $argv[2];
	$nb = $argv[3];
	
	$note = $root;
	for ($i = 0; $i < $nb; ++$i) {
		$incr[$i] = round($sz / ($sr / $note));
		$note *= 1.0594630943592952645618252949463417007792043174941856;
	}
	
	echo c_array("uint16_t", "g_incr", $incr, 12);
?>
