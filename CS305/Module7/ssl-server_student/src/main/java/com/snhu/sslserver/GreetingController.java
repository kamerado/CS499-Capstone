package com.snhu.sslserver;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.nio.charset.StandardCharsets;
import java.util.HexFormat;

@RestController
public class GreetingController {
	private static String genChecksum(String text, String algo) {
		MessageDigest digest;
		try {
			digest = MessageDigest.getInstance(algo);
		} catch (NoSuchAlgorithmException e) {
			digest = null;
			return null;
		}
		byte[] hashBytes = digest.digest(text.getBytes(StandardCharsets.UTF_8));
		return HexFormat.of().formatHex(hashBytes);
		
	}
	
    @GetMapping("/hash")
    public String myHash(){
    	String data = "Kameron Coolong!";
    	
    	String checksum = genChecksum(data, "SHA-256");
       
        return "<p>data:"+data+"<br/><br/>SHA-256 CheckSum Value:" + checksum;
    }
	
}