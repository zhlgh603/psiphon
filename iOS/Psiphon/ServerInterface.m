//
//  ServerInterface.m
//  Psiphon
//
//  Created by eugene on 2013-10-08.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "ServerInterface.h"
#import "PsiphonData.h"
#import "Utils.h"
#import "ServerEntry.h"
#import "EmbeddedValues.h"
#import "TunnelCore.h"

#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

@interface ParamPair : NSObject
-(id) initWithPair:(NSString*)key withValue:(NSString*)value;
@property  NSString* key;
@property  NSString* val;
@end

@implementation ParamPair
-(id) initWithPair:(NSString*)Key withValue:(NSString*)Value
{
    self = [self init];
    if(self)
    {
        self.key = Key;
        self.val = Value;
    }
    return self;
}
@synthesize key, val;
@end

@implementation ServerInterface

@synthesize delegate;

-(ServerEntry*) decodeServerEntry:(NSString *)encodedEntryString
{
    NSString *serverEntryString = [Utils dehexString:encodedEntryString];
    
    // Skip past legacy format (4 space delimited fields) and just parse the JSON config
    NSRange jsonRange;
    NSString* jsonString = serverEntryString;
    
    for(int i = 0; i < 4; i++)
    {
        jsonRange = [jsonString rangeOfString:@" "];
        if(jsonRange.location != NSNotFound && jsonRange.location < jsonString.length)
            jsonString = [jsonString substringFromIndex:jsonRange.location + 1];
        else
            return nil;
    }
    
    NSError *e = nil;
    NSDictionary *obj = [NSJSONSerialization JSONObjectWithData: [jsonString dataUsingEncoding:NSUTF8StringEncoding]
                                                        options: kNilOptions
                                                          error: &e
                         ];
    if(obj == nil)
        return nil;

    ServerEntry* newEntry = [[ServerEntry alloc] init];
    
    newEntry.encodedEntry = encodedEntryString;
    newEntry.ipAddress = [obj valueForKey:@"ipAddress"];
    newEntry.webServerPort = [[obj valueForKey:@"webServerPort"] integerValue];
    newEntry.webServerSecret = [obj valueForKey:@"webServerSecret"];
    newEntry.webServerCertificate = [obj valueForKey:@"webServerCertificate"];
    newEntry.sshPort = [[obj valueForKey:@"sshPort"] integerValue];
    newEntry.sshUsername = [obj valueForKey:@"sshUsername"];
    newEntry.sshPassword = [obj valueForKey:@"sshPassword"];
    newEntry.sshHostKey = [obj valueForKey:@"sshHostKey"];
    newEntry.sshObfuscatedPort = [[obj valueForKey:@"sshObfuscatedPort"] integerValue];
    newEntry.sshObfuscatedKey = [obj valueForKey:@"sshObfuscatedKey"];

    newEntry.capabilities = [[NSMutableArray alloc]initWithCapacity:4];
    
    NSArray* caps = [obj valueForKey:@"capabilities"];
    if(caps)
    {
        [newEntry.capabilities addObjectsFromArray:caps];
    }
    
    
    else
    {
        // At the time of introduction of the server capabilities feature
        // these are the default capabilities possessed by all servers.
        [newEntry.capabilities addObject:newEntry.CAPABILITY_OSSH];
        [newEntry.capabilities addObject:newEntry.CAPABILITY_SSH];
        [newEntry.capabilities addObject:newEntry.CAPABILITY_VPN];
        [newEntry.capabilities addObject:newEntry.CAPABILITY_HANDSHAKE];
    }
    
    return newEntry;
    
}

-(void) appendServerEntry:(NSString *)encodedEntry
{
    ServerEntry *newEntry = [self decodeServerEntry:encodedEntry];
    if (newEntry != nil && [newEntry hasCapability:RELAY_PROTOCOL])
    {
        // Check if there's already an entry for this server
        for (int i = 0; i < _serverEntries.count; i++)
        {
            ServerEntry* serverEntry = [_serverEntries objectAtIndex:i];
            if ([newEntry.ipAddress isEqualToString: serverEntry.ipAddress])
            {
                // This shouldn't be used on an existing list
                assert(false);
                break;
            }
        }
        [_serverEntries addObject:newEntry];
    }
}

-(void) insertServerEntry:(NSString*)encodedEntry withIsEmbedded:(BOOL)isEmbedded
{
    ServerEntry *newEntry = [self decodeServerEntry:encodedEntry];
    int existingIndex = -1;
    
    if (newEntry != nil)
    {
        // Check if there's already an entry for this server
        for (int i = 0; i < _serverEntries.count; i++)
        {
            ServerEntry* serverEntry = [_serverEntries objectAtIndex:i];
            if ([newEntry.ipAddress isEqualToString: serverEntry.ipAddress])
            {
                existingIndex = i;
                break;
            }
        }
        if(existingIndex != -1)
        {
            if(!isEmbedded)
            {
                [_serverEntries replaceObjectAtIndex: existingIndex withObject:newEntry];
            }
        }
        else
        {
            // New entries are added in the second position, to preserve
            // the first position for the "current" working server
            int index = _serverEntries.count > 0 ? 1 : 0;
            [_serverEntries insertObject:newEntry atIndex:index];
        }
    }
}

-(void) shuffleAndAddServerEntries:(NSArray*) encodedServerEntries withIsEmbedded:(BOOL)isEmbedded
{
    // Shuffling assists in load balancing when there
    // are multiple embedded/discovery servers at once
    
    [Utils shuffle:&encodedServerEntries];
    
    for (NSString* entry in encodedServerEntries)
    {
        [self insertServerEntry:entry withIsEmbedded:isEmbedded];
    }
}

-(void) saveServerEntries
{
    @synchronized([PsiphonData sharedInstance])
    {
        NSString* filepath = LIBRARY_PATH(SERVER_ENTRY_FILENAME);
        
        NSFileManager* fileManager = [NSFileManager defaultManager];
        
        if(![fileManager fileExistsAtPath:filepath])
        {
            [fileManager createFileAtPath:filepath contents:nil attributes:nil];
        }
        
        NSOutputStream *oStream = [[NSOutputStream alloc] initToFileAtPath:filepath append:NO];
        
        NSMutableArray *encodedEntries = [[NSMutableArray alloc] init];

        for( ServerEntry* entry in _serverEntries)
        {
            [encodedEntries addObject: entry.encodedEntry];
        }
        
        
        NSDictionary *obj = [[NSDictionary alloc] initWithObjectsAndKeys: encodedEntries, @"serverEntries", nil];

        if([NSJSONSerialization isValidJSONObject:obj])
        {
            [oStream open];
            NSError *e = nil;
            [NSJSONSerialization writeJSONObject: obj toStream:oStream
                                                                options: kNilOptions
                                                                  error: &e ];
            [oStream close];
            if(e != nil)
            {
                [Utils postLogEntryNotification:@"failed writing server entries to file with: %@", e];
            }
        }
        else
        {
            [Utils postLogEntryNotification:@"server entries dictionary is not a valid JSON object", nil];
        }
        //TODO: handle errors
    }
}

- (id)init {
    self = [super init];
    if (self) {
        @synchronized([PsiphonData sharedInstance])
        {
            _serverEntries = [[NSMutableArray alloc] init];
            
            NSFileManager* fileManager = [NSFileManager defaultManager];
            
            NSString* entriesFilePath = LIBRARY_PATH(SERVER_ENTRY_FILENAME);
            
            if(![fileManager fileExistsAtPath:entriesFilePath])
            {
                NSError *error = nil;
                NSString *bundledFilePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:SERVER_ENTRY_FILENAME];
                if([fileManager fileExistsAtPath:bundledFilePath])
                {
                    BOOL success = [fileManager copyItemAtPath:bundledFilePath toPath:entriesFilePath error:&error];
                    if (!success) {
                        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
                        abort();
                    }
                }
            }
            
            if([fileManager fileExistsAtPath:entriesFilePath])
            {
                NSArray *encodedEntries = nil;

                NSData *jsonData = [fileManager contentsAtPath:entriesFilePath];
                NSError *e = nil;
                NSDictionary *obj = [NSJSONSerialization JSONObjectWithData: jsonData options: kNilOptions error: &e];
                
                if(obj)
                {
                    encodedEntries = [obj valueForKey:@"serverEntries"];
                }
                if(encodedEntries)
                {
                    for (NSString* entry in encodedEntries)
                    {
                        // NOTE: No shuffling, as we're restoring a previously arranged list
                        [self appendServerEntry:entry];
                    }
                }
            }
        }
        
        NSUInteger numElements = 0;
        
        while(EMBEDDED_SERVER_LIST[numElements])
            numElements++;
        
        NSArray* embeddedServerList = [[NSArray alloc] initWithObjects:EMBEDDED_SERVER_LIST count:numElements];

        [self shuffleAndAddServerEntries:embeddedServerList withIsEmbedded:YES];
        [self saveServerEntries];
    }
    return self;
}

-(void)generateNewCurrentClientSessionID
{
    @synchronized(self)
    {
        NSMutableData* data = [NSMutableData dataWithLength:CLIENT_SESSION_ID_SIZE_IN_BYTES];
        int err = SecRandomCopyBytes(kSecRandomDefault, CLIENT_SESSION_ID_SIZE_IN_BYTES, [data mutableBytes]);

        if(err != noErr)
            @throw [NSException exceptionWithName:@"SecRandomCopyBytes" reason:@"failed" userInfo:nil];
        
        NSString* clientSessionIdString = [Utils NSDataToHex:data];
        
        [[PsiphonData sharedInstance] setClientSessionID:clientSessionIdString];
    }
}

-(ServerEntry*) setCurrentServerEntry
{
    // Saves selected currentServerEntry for future reference (e.g., used by getRequestURL calls)
    if (_serverEntries.count > 0)
    {
        _currentServerEntry = [[_serverEntries objectAtIndex:0] copy];
        return _currentServerEntry;
    }
    return nil;
}

-(ServerEntry*)  getCurrentServerEntry
{
    if (!_currentServerEntry)
    {
        return nil;
    }
    return [_currentServerEntry copy];
}

-(NSArray*) getServerEntries
{
    if (_serverEntries && _serverEntries.count > 0)
    {
        _currentServerEntry = [[_serverEntries objectAtIndex:0] copy];
        return [_serverEntries copy];
    }
    return nil;
}

-(NSData*) PEMToDER:(NSString*) PEMString
{
	if ([PEMString rangeOfString:@"BEGIN"].location == NSNotFound) {
		//did not find the "-----BEGIN CERTIFICATE-----" header, so it must be a DER encoded x509 cert
		return [NSData dataFromBase64String:PEMString];
	}
	//it must be a base64 encoded x509 then
    
    //remove header and footer comments
    PEMString = [PEMString stringByReplacingOccurrencesOfString:@"-----BEGIN CERTIFICATE-----" withString:@""];
    PEMString = [PEMString stringByReplacingOccurrencesOfString:@"-----END CERTIFICATE-----" withString:@""];
	
	//strip the whitespace
	NSString *cleanKey = @"";
	NSArray *chunks = [PEMString componentsSeparatedByString:@"\n"];
	for (int i=0; i<[chunks count]; i++) {
		NSString *chunk = [chunks objectAtIndex:i];
        //certifcate line, clean whitespaces en newlines
        NSString *line = [chunk stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        //check if line is not empty
        if ([line length] > 0) {
            cleanKey = [NSString stringWithFormat:@"%@%@", cleanKey, line];
		}
	}
    
	return[NSData dataFromBase64String:cleanKey];
}

-(NSString*) DERtoPEM:(NSData*) DERData
{
    size_t outputLength = 0;
	char *outputBuffer =
    mk_NewBase64Encode([DERData bytes], [DERData length], true, &outputLength);
	
	NSString *result =
    [[NSString alloc]
     initWithBytes:outputBuffer
     length:outputLength
     encoding:NSASCIIStringEncoding];
	free(outputBuffer);
    
    return [NSString stringWithFormat:@"-----BEGIN PUBLIC KEY-----\n%@\n-----END PUBLIC KEY-----\n", result];
}

-(BOOL) handleCertificateWithChallenge:(NSURLAuthenticationChallenge*) challenge andConnection:(NSURLConnection *)connection
{
    DLog(@"Auth Method %@", challenge.protectionSpace.authenticationMethod);
    if(challenge.protectionSpace.authenticationMethod != NSURLAuthenticationMethodServerTrust)
        return NO;
    
    NSData *expectedCertData = [self PEMToDER:_currentServerEntry.webServerCertificate];
    
    SecTrustRef trustRef = [[challenge protectionSpace] serverTrust];
    SecTrustEvaluate(trustRef, NULL);
    CFIndex count = SecTrustGetCertificateCount(trustRef);
    
    NSData* actualCertData;
    SecCertificateRef certRef;
    
    if(count > 0)
    {
        certRef = SecTrustGetCertificateAtIndex(trustRef, 0);
        actualCertData = (__bridge_transfer NSData*)SecCertificateCopyData(certRef);
        
        if([expectedCertData isEqualToData:actualCertData])
        {
            [challenge.sender useCredential:[NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust] forAuthenticationChallenge:challenge];
            return YES;
        }
    }
    [challenge.sender performDefaultHandlingForAuthenticationChallenge:challenge];
    return YES;
}

-(void) fetchRemoteServerList
{
    
    NSDate* nextFetchRemoteServerListDate = [[PsiphonData sharedInstance] nextFetchRemoteServerListDate];
    NSDate* now = [NSDate date];
    
    if(nextFetchRemoteServerListDate != nil &&
       [nextFetchRemoteServerListDate compare:now] == NSOrderedDescending )
    {
        return;
    }
    // Send a synchronous request to keep things simple, let default URL loading system
    // handle SSL. Note that this request is not handled by ProxyURLProtocol
    // see canInitWithRequest method of ProxyURLProtocol
    
    [[PsiphonData sharedInstance] setNextFetchRemoteServerListDate:
        [[NSDate alloc]initWithTimeIntervalSinceNow:SECONDS_BETWEEN_UNSUCCESSFUL_REMOTE_SERVER_LIST_FETCH]];

    
    NSURLRequest * urlRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:REMOTE_SERVER_LIST_URL]];
    NSURLResponse * response = nil;
    NSError * error = nil;
    NSData * data = [NSURLConnection sendSynchronousRequest:urlRequest
                                          returningResponse:&response
                                                      error:&error];
    
    if (error == nil)
    {
        NSString* encodedServersString = [self validateAndExtractData:data
                                                           withPubKey:REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY
                                                                error:&error];
        if(error == nil)
        {
            NSArray* encodedServerEntries = [encodedServersString componentsSeparatedByString:@"\n"];
            [self shuffleAndAddServerEntries:encodedServerEntries withIsEmbedded:FALSE];
            [self saveServerEntries];
            [[PsiphonData sharedInstance]setNextFetchRemoteServerListDate:
                [[NSDate alloc]initWithTimeIntervalSinceNow:SECONDS_BETWEEN_SUCCESSFUL_REMOTE_SERVER_LIST_FETCH]];
        }
        else
        {
            [Utils postLogEntryNotification: @"remote list extraction failed with %@", error];
        }
    }
    else
    {
        [Utils postLogEntryNotification: @"fetch remote list failed with %@", error];
    }
}

-(NSString*) validateAndExtractData:(NSData*)jsonData withPubKey:(NSString*)pubKey error:(NSError**) errorPtr
{
    NSDictionary *obj = [NSJSONSerialization JSONObjectWithData: jsonData options: kNilOptions error: errorPtr];
    
    if(! obj)
        return nil;
    
    NSString *data = [obj valueForKey:@"data"];
    NSString* signature = [obj valueForKey:@"signature"];
    NSString* signingPublicKeyDigest = [obj valueForKey:@"signingPublicKeyDigest"];
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    //public key SHA256 digest
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, (unsigned char*)[pubKey UTF8String], (int)[pubKey length]);
    SHA256_Final(hash, &sha256);
    
    //do we have the required key?
    BOOL isRightKey = [[NSData dataWithBytes:hash length:SHA256_DIGEST_LENGTH] isEqualToData:[NSData dataFromBase64String:signingPublicKeyDigest]];
    
    if(!isRightKey)
    {
        return nil;
    }
    
    NSData* signatureData = [NSData dataFromBase64String:signature];
    
    //data SHA256 digest
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, (unsigned char*)[data UTF8String], (int)[data length]);
    SHA256_Final(hash, &sha256);
    
    
    //format proper PEM encoded key &
    // init RSA with it
    
    NSData* derKey = [NSData dataFromBase64String:pubKey];
    
    NSString* pemKey = [self DERtoPEM:derKey];

    const char *p = (char *)[pemKey UTF8String];
    
    BIO *bufio;
    RSA *rsa;
    
    bufio = BIO_new_mem_buf((void*)p, -1);
    rsa = PEM_read_bio_RSA_PUBKEY(bufio, 0, 0, 0);
    
    if(!rsa)
    {
        return nil;
    }
    
    //verify signature
    BOOL signatureVerified = (BOOL)RSA_verify(NID_sha256, hash, SHA256_DIGEST_LENGTH,
               (unsigned char *)[signatureData bytes], (unsigned int)[signatureData length], rsa);
    
    if(signatureVerified)
    {
        return data;
    }
    return nil;
}


- (void) doGETRequestWithPath:(NSString*)path
            completionHandler:(PsiphonServerResponseBlock) completionBlock
                 errorHandler:(PsiphonServerErrorBlock) errorBlock
                  extraParams:(NSArray*) extraParams
{
    NSString* strURL = [self getRequestURL:path extraParams:extraParams];
    
    
    NSString* engineHost = [NSString stringWithFormat:@"%@:%d",
                            _currentServerEntry.ipAddress,
                            _currentServerEntry.webServerPort];

    if(!_engine || ![[_engine readonlyHostName] isEqualToString:engineHost])
    {
        [_engine cancelAllOperations];
        
        _engine = [[MKNetworkEngine alloc]
                               initWithHostName:engineHost customHeaderFields:nil];
    }
    
    MKNetworkOperation* op = [_engine
                              operationWithPath:strURL params: nil
                              httpMethod:@"GET" ssl:YES];
    
    __weak ServerInterface *myself = self; //avoid strong reference cycle trick
    
    op.certificateHandler = ^BOOL(NSURLAuthenticationChallenge* challenge, NSURLConnection *connection){
        
        return [myself handleCertificateWithChallenge:challenge andConnection:connection];
    };
    
    
    [op addCompletionHandler:^(MKNetworkOperation *operation) {
        
        if(completionBlock != nil)
            completionBlock([operation responseString]);
    } errorHandler:^(MKNetworkOperation *errorOp, NSError* error) {
        if(errorBlock != nil)
            errorBlock(errorOp, error);
    }];
    
    [_engine enqueueOperation:op];
}

-(NSString*) getRequestURL:(NSString*)path extraParams:(NSArray *)params
{
    NSMutableString *str =[NSMutableString string];
    
    [str appendString:path];
    [str appendString:@"?client_session_id="];
    [str appendString: [Utils urlEncode:[[PsiphonData sharedInstance] clientSessionID]] ];
    [str appendString:@"&server_secret="];
    [str appendString: [Utils urlEncode:[_currentServerEntry webServerSecret]]];
    [str appendString:@"&propagation_channel_id="];
    [str appendString: [Utils urlEncode: PROPAGATION_CHANNEL_ID]];
    [str appendString:@"&sponsor_id="];
    [str appendString: [Utils urlEncode: SPONSOR_ID]];
    [str appendString:@"&client_version="];
    [str appendString: [Utils urlEncode: CLIENT_VERSION]];
    [str appendString:@"&relay_protocol="];
    [str appendString: [Utils urlEncode:[[PsiphonData sharedInstance] tunnelRelayProtocol]] ];
    [str appendString:@"&client_platform="];
    [str appendString: [Utils urlEncode: CLIENT_PLATFORM]];
    
    
    if(params !=nil)
    {
        for (ParamPair* pair in params)
        {
            [str appendString:@"&"];
            [str appendString:pair.key];
            [str appendString:@"="];
            [str appendString:[Utils urlEncode: pair.val]];
        }
    }
    
    return[NSString stringWithString:str];
}

-(void)handleHandshakeResponse:(NSString*) response
{
    
    NSString* const JSON_CONFIG_PREFIX = @"Config: ";
    NSString* jsonString;
    
    NSRange configRange;
    
    for (NSString* string in [response componentsSeparatedByString:@"\n"])
    {
        
        configRange = [string rangeOfString:JSON_CONFIG_PREFIX];
        if(configRange.location != NSNotFound && configRange.location == 0)
            jsonString = [string substringFromIndex:configRange.location + [JSON_CONFIG_PREFIX length]];
    }
    
    NSError *e = nil;
    NSDictionary *obj = [NSJSONSerialization
                         JSONObjectWithData:[jsonString dataUsingEncoding:NSUTF8StringEncoding]
                         options: kNilOptions
                         error: &e
                         ];
    
    if(obj != nil)
    {
        NSArray* homepages = [obj valueForKey:@"homepages"];
        if(homepages && homepages.count)
        {
            [[PsiphonData sharedInstance] setHomePages:homepages];
        }
        NSArray* encoded_server_list = [obj valueForKey:@"encoded_server_list"];
        if(encoded_server_list && encoded_server_list.count)
        {
            [self shuffleAndAddServerEntries:encoded_server_list withIsEmbedded:NO];
        }
        
        NSString* ssh_session_id = [obj valueForKey:@"ssh_session_id"];
        
        if(ssh_session_id)
        {
            [[PsiphonData sharedInstance] setTunnelSessionID:ssh_session_id];

        }
    }
    
    [delegate handshakeSuccess:response];
}

-(void) handleHandshakeError: (MKNetworkOperation*) completedOperation withError:  (NSError*) error
{
    //TODO: implement this
    
}

-(void)sendHandshakeRequest
{
    NSMutableArray* knownServers = [[NSMutableArray alloc] init];
    
    for (ServerEntry* entry in _serverEntries)
    {
        ParamPair* newParam = [[ParamPair alloc] init];
        newParam.key = @"known_server";
        newParam.val = entry.ipAddress;
        [knownServers addObject:newParam];
    }
    

    __weak ServerInterface *myself = self; //avoid strong reference cycle trick
    
    PsiphonServerResponseBlock completionBlock = ^void (NSString* response)
    {
        [myself handleHandshakeResponse: response];
    };
    
    PsiphonServerErrorBlock errorBlock = ^void(MKNetworkOperation* completedOperation, NSError* error)
    {
        [myself handleHandshakeError: completedOperation withError: error];
        
    };
    
    [self doGETRequestWithPath: @"/handshake" completionHandler:completionBlock errorHandler: errorBlock extraParams:knownServers];
}

-(void) handleConnectedResponse: (NSString*) response
{
    NSError *e = nil;
    
    NSDictionary *obj = [NSJSONSerialization
                         JSONObjectWithData:[response dataUsingEncoding:NSUTF8StringEncoding]
                         options: kNilOptions
                         error: &e
                         ];
    
    if(obj != nil)
    {
        NSString* last_connected = [obj valueForKey:@"connected_timestamp"];
        if(last_connected)
        {
            NSString* filepath = LIBRARY_PATH(LAST_CONNECTED_FILENAME);
            [[NSFileManager defaultManager] createFileAtPath:filepath
                                                contents:[last_connected dataUsingEncoding:NSUTF8StringEncoding]
                                                  attributes:nil];
        }
    }
}

-(void) handleConnectedError: (MKNetworkOperation*) completedOperation withError:  (NSError*) error
{
    //TODO: implement this
    
}



-(void) sendConnectedRequest
{
    NSString* lastConnected = @"None";
    
    NSString* filepath = LIBRARY_PATH(LAST_CONNECTED_FILENAME);
    NSFileManager* fileManager = [NSFileManager defaultManager];
    
    if([fileManager fileExistsAtPath:filepath])
    {
        lastConnected = [NSString stringWithContentsOfFile:filepath encoding:NSUTF8StringEncoding error:nil];
    }
    

    NSMutableArray* connectedParams = [[NSMutableArray alloc] init];
    
    ParamPair* newParam = [[ParamPair alloc]initWithPair:@"session_id"
                                               withValue:[[PsiphonData sharedInstance] tunnelSessionID]];
    [connectedParams addObject:newParam];
    
    newParam = [[ParamPair alloc]initWithPair:@"last_connected" withValue:lastConnected];
    [connectedParams addObject:newParam];
    

    __weak ServerInterface *myself = self; //avoid strong reference cycle trick
    
    PsiphonServerResponseBlock completionBlock = ^void (NSString* response)
    {
        [myself handleConnectedResponse: response];
    };
    
    PsiphonServerErrorBlock errorBlock = ^void(MKNetworkOperation* completedOperation, NSError* error)
    {
        [myself handleConnectedError: completedOperation withError: error];
    };
    
    [self doGETRequestWithPath: @"/connected" completionHandler:completionBlock errorHandler: errorBlock extraParams:connectedParams];
}

-(BOOL) serverWithCapabilitiesExists:(NSArray*) capabilities
{
    @synchronized(self)
    {
        for (ServerEntry* entry in _serverEntries)
        {
            if ([entry hasCapabilities:capabilities])
            {
                return YES;
            }
        }
        return NO;
    }
}

-(void) moveEntriesToFront:(NSArray*) reorderedServerEntries
{
    @synchronized(self)
    {
        // Insert entries in input order
        
        for (int i = [reorderedServerEntries count] - 1; i >= 0; i--)
        {
            ServerEntry* reorderedEntry = [reorderedServerEntries objectAtIndex:i];
            // Remove existing entry for server, if present. In the case where
            // the existing entry has different data, we must assume that a
            // discovery has happened that overwrote the data that's being
            // passed in. In that edge case, we just keep the existing entry
            // in its current position.
            
            
            for (int j = 0; j < [_serverEntries count]; j++)
            {
                ServerEntry* existingEntry = [_serverEntries objectAtIndex:j];
                
                if ([reorderedEntry.ipAddress isEqualToString:existingEntry.ipAddress])
                {
                    // NOTE: depends on encodedEntry representing the entire object
                    if ([reorderedEntry.encodedEntry isEqualToString:existingEntry.encodedEntry])
                    {
                        //move entry to front
                        [_serverEntries exchangeObjectAtIndex:j withObjectAtIndex:0];
                    }
                    break;
                }
            }
        }
        
        [self saveServerEntries];
    }
}


@end
