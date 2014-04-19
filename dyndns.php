<?php
header('Content-Type: text/plain');
$subdomain = '.fill.me.in.'; // don't forget the final dot
$prefix = '/dyndns/';
$hostname = explode('.', $_SERVER['QUERY_STRING'], 2);
$ip = $_SERVER['REMOTE_ADDR'];
$filename = substr($hostname[0],0,10).$subdomain;
print 'Registering '.$ip.' as '.$filename.'... ';
file_put_contents($prefix.$filename,$ip."\n");
print 'done.'
?>
