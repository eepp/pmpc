<?php
	/**
	 * this script takes a plain-text pmpc score file as an input and
	 * produces binary tracks in C array form. execute with no arguments
	 * for usage. no error verification on arguments yet.
	 *
	 * author:	Philippe Proulx <eepp.ca>
	 * date:	2012/03/17
	 */
	define('NOTE_TYPE_NORMAL', 0);
	define('NOTE_TYPE_PAUSE', 1);
	define('LENGTH_TYPE_TICKS', 0);
	define('LENGTH_TYPE_AUTO', 1);
	
	function print_usage() {
		global $argv;
		
		printf("usage: php %s <input>\n" .
			"  <input>  plain-text pmpc score\n", $argv[0]);
	}
	
	function parse_score_from_file($filename) {
		$cont = file_get_contents($filename);
		$lines = explode("\n", $cont);
		
		// ignore comments and get global informations
		$gl = array();
		$meta = array();
		foreach ($lines as $line) {
			if (preg_match('/^\\s*#/', $line)) {
				continue;
			}
			if (preg_match('/^\\s*>/', $line)) {
				if (preg_match('/>\\s*(\\w+):\\s*(.+)$/', trim($line), $m)) {
					switch ($m[1]) {
						case 'samps_per_tick':
						$meta['samps_per_tick'] = intval($m[2]);
						break;
						
						case 'generators':
						$meta['generators'] = preg_split('/\\s*,\\s*/', trim($m[2]));
						foreach ($meta['generators'] as $g) {
							if (!in_array($g, array('tri', 'sq', 'saw', 'noise32k', 'noise93'))) {
								error_exit(sprintf('parse error: unknown generator "%s"', $g), 2);
							}
						}
						break;
						
						case 'ticks_per_whole':
						$meta['ticks_per_whole'] = intval($m[2]);
						break;
						
						case 'cut_ticks':
						$meta['cut_ticks'] = intval($m[2]);
						break;
						
						default:
						error_exit(sprintf('parse error: unknown key "%s"', $m[1]), 2);
						break;
					}
				}
				continue;
			}
			$gl[] = $line;
		}
		if (count($meta) != 4) {
			error_exit("parse error: missing metadata", 2);
		}
		
		// join tracks
		$good = trim(implode("\n", $gl));
		$blocks = preg_split('/\\n{2,}/', $good);
		foreach ($blocks as $k => $block) {
			$l = explode("\n", $block);
			if ($k == 0) {
				$tracks = $l;
				continue;
			}
			foreach ($l as $kl => $vl) {
				$tracks[$kl] .= " $vl";
			}
		}
		
		// split tokens
		for ($i = 0; $i < count($tracks); ++$i) {
			$score[$i] = array();
		}
		foreach ($tracks as $k => $track) {
			$tokens = preg_split('/\\s+/', trim($track));
			$notes = array();
			foreach ($tokens as $token) {
				$add = array();
				$add['token'] = $token;
				if (preg_match('%^([cCdDeEfFgGaAbB][012345]|/)([=-])(.+)$%', $token, $m)) {
					$add['cut'] = preg_match('/\\+$/', $token);
					$add['len'] = $m[3];
					switch ($m[2]) {
						// length in ticks
						case '-':
						$add['lt'] = LENGTH_TYPE_TICKS;
						break;
					
						// computed length
						case '=':
						$add['lt'] = LENGTH_TYPE_AUTO;
						break;
					}
					if ($m[1] == '/') {
						// pause
						$add['nt'] = NOTE_TYPE_PAUSE;
					} else {
						// normal note
						preg_match('/([cCdDeEfFgGaAbB])([012345])/', $m[1], $mm);
						$add['nt'] = NOTE_TYPE_NORMAL;
						$add['note'] = $mm[1];
						$add['oct'] = $mm[2];
					}
				} else {
					error_exit(sprintf('parse error: invalid token "%s"', $token), 2);
				}
				$score[$k][] = $add;
			}
		}
		
		return array($score, $meta);
	}
	
	function error_exit($msg, $exit_code) {
		fprintf(STDERR, "error: %s\n", $msg);
		exit($exit_code);
	}
	
	function ticks_from_auto($len, $ticks_per_whole) {
		if (preg_match('/^\d+\\+?$/', $len)) {
			// simple note
			return $ticks_per_whole / intval($len);
		} else if (preg_match('/^(\d+).\\+?$/', $len, $m)) {
			// dotted note
			return ($ticks_per_whole / intval($m[1])) * 1.5;
		} else if (preg_match('/^(\d+),(\d+)\\+?$/', $len, $m)) {
			// tuplet note
			return ($ticks_per_whole / intval($m[1])) / intval($m[2]);
		} else {
			// unknown note
			error_exit(sprintf('parse error: invalid duration "%s"', $len), 2);
		}
	}
	
	function bin_from_score($score, $meta) {
		$notes_incr_map = array(
			'c' => 0,
			'C' => 1,
			'd' => 2,
			'D' => 3,
			'e' => 4,
			'E' => 5,
			'f' => 5,
			'F' => 6,
			'g' => 7,
			'G' => 8,
			'a' => 9,
			'A' => 10,
			'b' => 11,
			'B' => 12
		);
		$bin_tracks = array();
		foreach ($score as $track) {
			$bin_track = array();
			foreach ($track as $tok) {
				extract($tok);
				$l = $len;
				if ($lt == LENGTH_TYPE_AUTO) {
					$l = ticks_from_auto($len, $meta['ticks_per_whole']);
				}
				if ($cut) {
					$l -= $meta['cut_ticks'];
				}
				switch ($nt) {
					case NOTE_TYPE_PAUSE:
					if ($l > 127) {
						error_exit(sprintf('parse error: invalid length %d for token "%s"', $l, $token), 2);
					}
					$bin_track[] = (128 + $l);
					break;
					
					case NOTE_TYPE_NORMAL:
					if ($l > 255) {
						error_exit(sprintf('parse error: invalid length %d for token "%s"', $l, $token), 2);
					}
					$bin_track[] = ($notes_incr_map[$note] + (12 * $oct));
					$bin_track[] = $l;
					break;
					
					default:
					error_exit(sprintf('fatal error: unknown note type %d', $nt));
					break;
				}
				if ($cut) {
					$bin_track[] = (128 + $meta['cut_ticks']);
				}
			}
			$bin_tracks[] = $bin_track;
		}
		
		return $bin_tracks;
	}
	
	function c_array($pref, $values, $sep) {
		$ret = "$pref = {";
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
	
	if ($argc != 2) {
		print_usage();
		exit(1);
	}
	
	// get score structure
	list($score, $meta) = parse_score_from_file($argv[1]);
	
	// create binary image
	$bin = bin_from_score($score, $meta);
	
	// convert to C file
	printf("#define TRACKS_SZ	%d\n", count($bin));
	printf("#define SAMPS_PER_TICK	%d\n", $meta['samps_per_tick']);
	printf("#define VOL_DIV_EXP	%d\n", ceil(log(count($bin), 2)));
	echo "\n";
	$names = array();
	$szs = array();
	$gens = array();
	foreach ($bin as $k => $bin_track) {
		// data
		$name = "g_track_data$k";
		$names[] = $name;
		
		
		// size
		$sz = count($bin_track);
		$szs[] = $sz;
		
		// generator
		$gen = sprintf("gen_%s", $meta['generators'][$k]);
		$gens[] = $gen;
				
		echo c_array("static prog_uint8_t $name []", $bin_track, 16);
	}
	echo c_array("static uint8_t* g_tracks_datas []", $names, 8);
	echo c_array("static uint16_t g_tracks_sizes []", $szs, 8);
	echo c_array("static uint8_t (*g_tracks_generators [])(struct track*)", $gens, 8);
?>

