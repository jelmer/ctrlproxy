<?

function mycmp($v1,$v2)
{
	global $sortfield;
	if ($v1[$sortfield] == $v2[$sortfield]) return 0;
    return ($v1[$sortfield] > $v2[$sortfield]) ? -1 : 1;
}

function sortmydata(&$data, $field)
{
	global $sortfield;
	$sortfield = $field;
	uasort(&$data, "mycmp");
}


$network = "INU";
$channel = "#ctrlproxy";

$fd = popen("../printstats /home/jelmer/.ctrlproxy-stats.tdb", "r");

while(!feof($fd)) {
	$v = fgets($fd);
	preg_match("'([^/]*)\/([^/]*)\/([^/]*)\\/([^/]*): ([0-9]+)'", $v, $regs);
	if($regs[0] == "")continue;
	$data[$regs[1]][$regs[2]][$regs[3]][$regs[4]] = $regs[5];
}

fclose($fd);

/* Okay, all data has been read. Now build a nice page based on it. */

$mydata = $data[$network][$channel];

/* Add some statistics to the array */
reset($mydata);
while(list($k,$v) = each($mydata)) {
	if($v["lines"] == 0 || $v["words"] == 0)continue;
	$mydata[$k]["avg_words_per_line"] = $v["words"] / $v["lines"];
	$mydata[$k]["avg_chars_per_line"] = $v["chars"] / $v["lines"];
}

?>
<html>
<head>
<title><?=$channel;?> @ <?=$network;?> stats by ctrlproxy</title>
</head>
<body bgcolor="#ffffff">

<h1><?=$channel;?> @ <?=$network;?> stats by ctrlproxy</h1>

<? if(isset($extended)) { ?>
<h2><?=$title;?></h2>

<table border=0>

<? if($extended == "lines") { ?>

<tr><td></td><td><b>Nick</b></td><td><b>Lines</b></td><td><b>Words</b></td></tr>
<? sortmydata($mydata, "lines"); 
reset($mydata);$i = 0;
while(list($k,$v) = each($mydata)) {
	if(!$k)continue;
	$i++;
	print "<tr><td>$i</td><td>$k</td><td>" . $v["lines"] . "</td><td>" . $v["words"] . "</td></tr>\n";
}
?><p>Total lines: <?=$mydata[""]["lines"]; 
} else {
?>

<tr><td></td><td><b>Nick</b></td><td><b><?=$extended;?></b></td></tr>
<? sortmydata($mydata, $extended);
reset($mydata);$i = 0;
while(list($k, $v) = each($mydata)) {
	if(!$k)continue;
	$i++;
	print "<tr><td>$i</td><td>$k</td><td>" . $v[$extended] . (isset($showprcnt)?(" (" . round($v[$extended] / $v["lines"] * 100) . "%)"):"") . "</td></tr>\n";
}

}
?>

</table>

<?
} else {
$fields = array();
$fields["lines"] = array("title" => "Lines", "desc1" => "%s wrote the most lines: %d", "desc2" => "%s wrote almost as much lines: %d");
$fields["foul"] = array("title" => "Foul Language", "desc1" => "%s makes sailors blush", "desc2" => "%s has a potty mouth as well");
$fields["happy"] = array("title" => "Happy", "desc1" => "%s is the happiest person on the channel", "desc2" => "%s is quite happy as well");
$fields["unhappy"] = array("title" => "Unhappy", "desc1" => "%s is the saddest person on the channel", "desc2" => "%s is quite sad as well");


function expand_desc($desc, $k, $v)
{
	return strtr($desc, array("%s" => $k, "%d" => $v));
}

reset($fields);
while(list($k,$v) = each($fields)) {
	sortmydata($mydata, $k);
	reset($mydata);
	echo "<h3>" . $v["title"] . "</h3>\n";
	list($k1,$v1) = each($mydata);
	if(isset($v["desc1"])) echo "<p>" . expand_desc($v["desc1"], $k1, $v1[$k]) . "\n";
	list($k1,$v1) = each($mydata);
	if(isset($v["desc2"])) echo "<p>" . expand_desc($v["desc2"], $k1, $v1[$k]) . "\n";
	echo "<p><a href='$PHP_SELF?extended=$k&title=" . urlencode($v["title"]) . (isset($v["showpercent"])?"&showprcnt=1":"") . "'>...</a>";
	echo "<hr>\n";

}

}
?>

</body>
</html>
