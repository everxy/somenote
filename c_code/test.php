<?php
	
	$content = file_get_contents('php://input');
	file_put_contents("docker_manager.log","content :".$content."\n",FILE_APPEND);
	include "../dbconnect.php";

	
	$action = getValue($content,'action','0');
	switch ($action) {
		case 'startapp':
			startapp($content);
			break;		
		case 'update':
			updateapp($content);
			break;
		case 'closeapp':
			closeapp($content);
			break;
		case 'userexit':
			userexit($content);
			break;
		default:
			# code...
			break;
	}




	function userexit($content){
		$userid = getValue($content,'userid','0');
		$sql_of_open = "SELECT * from dynamicApp where userid=$userid";
		$res_of_open = mysql_query($sql_of_open);
		while (($ss = mysql_fetch_array($res_of_open))) {
			$url = "http://" . $ss['ip'] ."/docker/docker_rec.php";
			$data = "<root><action>closeapp</action><dockerid>".$ss['dockerid']."</dockerid></root>";
			$response = QueryData($url,$data);
			file_put_contents("docker_manager.log","exit:\n"$url."\n".$data."\n".$response."\n",FILE_APPEND);
			$status = getValue($response,"status","0");
			if($status == 'success'){
				$sql_of_del = "DELETE from dynamicApp where dockerid='$ss[3]'";
				echo $sql_of_del;
				$res = mysql_query($sql_of_del);
			}

		}
	}



	function closeapp($content){
		
		$appid = getValue($content,'appid','0');
		$ip = getValue($content,'ip','0');
		$dockerid = getValue($content,'dockerid','0');

		$url = "http://" . $ip ."/docker/docker_rec.php"; //未完待续
		$data_close = "<root><action>closeapp</action><dockerid>$dockerid</dockerid></root>";
		$response = QueryData($url,$data_close);
		$status = getValue ($response,"status","0");
		if($status == 'success'){
			$sql = "DELETE from dynamicApp where dockerid='$dockerid'";
			mysql_query($sql);
		}

	}



	function startapp($content){
		//echo "startapp";
		$appid = getValue($content,'appid','0');
		$userid = getValue($content,'userid','0');
		$ppi = getValue($content,'ppi','0');

		// 判断app类型
		// linux应用才能操作，
		// 否则返回类型错误
		$sql_of_apptype = "SELECT apptype from app where appid=$appid";
		$query_apptype = mysql_query($sql_of_apptype);
		$res = mysql_fetch_array($query_apptype);
		//echo "res['apptype'] = " . $res['apptype'] . "\n";
		if($res['apptype'] != 'lca'){
			echo "<root><status>error</status><message>type</message></root>";
			file_put_contents("docker_manager.log","不是lca应用:".$res['apptype']."\n",FILE_APPEND);
			return ;
		}
	die();	
		// 查询网盘是否挂载成功，如果失败直接返回。
/*

	

		$getDisk = "userid=$userid";
		$disk_url = "http://127.0.0.1/cloudos/cloudstorage/query_mountpoint.php";
		$disk_res = QueryData($disk_url,$getDisk);
		if(($error_disk = getValue($disk_res,'error','0')) != ''){
			echo "<root><status>error</status><message>disk</message></root>";
			file_put_contents("docker_manager.log","查询网盘失败 :".$error_disk."\n",FILE_APPEND);
			return;
		}
			
*/





		$ips = getDockerIP($appid);
		
		foreach ($ips as $key => $value) {
			//向每个dockerip发送请求，成功返回，失败继续下一个ip
			$docker_url = "http://" . $value ."/docker/docker_rec.php"; //未完待续
			//$diskdata = parseData($disk_res);
			//$toDocker = "<root><action>startapp</action><userid>$userid</userid><appid>$appid</appid><ppi>$ppi</ppi>$diskdata</root>";
			$toDocker = "<root><action>startapp</action><userid>$userid</userid><appid>$appid</appid><ppi>$ppi</ppi></root>";
			$docker_back = QueryData($docker_url,$toDocker);
			$retcode = getValue ($docker_back,"status",0);
			//file_put_contents("docker_manager.log","docker_open_back:".$docker_url."\n".$toDocker."\n".$docker_back."\n",FILE_APPEND);
			if($retcode == 'success'){ //有一个成功即可，不需要继续执行
				echo $docker_back;
				file_put_contents("docker_manager.log","startapp,info:".$docker_back."\n",FILE_APPEND);
				return ;
			}
		}

		//如果全部失败，则报错
		echo "<root><status>error</status><message>usage</message></root>";
		file_put_contents("docker_manager.log","all docker is error\n",FILE_APPEND);
		return ;

	}



	function updateapp($content)
	{
		echo $content. " \n";
		$appid = getValue($content,'appid','0');
		$userid = getValue($content,'userid','0');
		$ip = getValue($content,'ip','0');
		$dockerid = getValue($content,'dockerid','0');

		// 获取已经打开的appinfo
		$sql_of_open = "INSERT into dynamicApp (userid,appid,dockerid,ip) values ('$userid','$appid','$dockerid','$ip')";
		echo "sql_of_open : " . $sql_of_open;
		$query = mysql_query($sql_of_open);

	}



	//查询appid对应的所有的dockerip，返回数组。
	function getDockerIP($appid){
		$sql = "SELECT vm.vmip from tplapp,tpl,vm where tplapp.appid=$appid and tplapp.tplid=tpl.tplid and vm.vmtpl=tpl.tplname";
		$res = mysql_query($sql);
		$ips = array();
		while (($ss = mysql_fetch_array($res))) {
			array_push($ips,$ss[0]);
		}
		return $ips;
	}

	// 获取xml中para标签的值,多个para标签节点，则用i标识第几个
	function getValue ($xml,$para,$i) {
		$dom = new DOMDocument();
		$dom -> loadXML($xml);
		$value = $dom -> getElementsByTagName($para) -> item($i) -> nodeValue;
		return $value;
	}

	function QueryData($url,$data)
	{
		$ch = curl_init();
		curl_setopt($ch,CURLOPT_URL,$url);
		curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
		curl_setopt($ch,CURLOPT_CUSTOMREQUEST,"POST");
		curl_setopt($ch,CURLOPT_POSTFIELDS,$data);
		$response = curl_exec($ch);
		curl_close($ch);
		return $response;
	}

	 
	function parseData($response){
		$xml = simplexml_load_string($response);
		$data = array();
		$i = 0;
		foreach ($xml->children() as $child) {
			if($child->getName() == 'ldappwd')
				$data[$i] = (string)$child;
			if($child->getName() == 'disk'){
				foreach ($child as $value) {
					$data[$i][$value->getName()] = (string)$value;
				}
			}
			$i++;
		}

		$str = "<netdisk>";
		$str .= "<ldappwd>$data[0]</ldappwd>";
		$j = 1;
		while ($data[$j]) {
			$str .= "<disk>";
			foreach ($data[$j] as $key => $value) {
				$str .= "<$key>$value</$key>";
			}
			$str .= "</disk>";
			$j++;
		}
		return $str . "</netdisk>";
	}

?>
