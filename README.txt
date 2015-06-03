Instructions to execute the code.

Initial steps to setup the system:


1. Create a database and run the database code to define the database tables.

2. DDL Code to execute the database is available in Database_microblogging.

3. Update the database server details in database server string in the server codes of serv1.c and serv2.c. 

4. Run make to compile all the files in the folder.


5. Executing the loadbalancer first.

In our design we have defined the loadbalancer service on port number 8001.


run the following command to execute loadbalancer code.

./loadbalancer <localhost name> 8001

ex:
./loadbalancer localhost 8001


6. Executing the servers:
 
SERVER 1:PRIMARY SERVER
we have defined the servers to take in the port number as arguments.


run the following command to execute server1 code: 

./serv1 <port No of server you choose>

ex : 

./serv1 6001


7.  Executing the servers:
 
SERVER 2:SECONDARY SERVER

we have defined the servers to take in the port number as arguments.


run the following command to execute server2 code: 


./serv2 <port No of server you choose>

ex : 
./serv2 7001



8. Execute the client: 


we have defined the client to take in the port number of the loadbalancer as argument.
run the following command to execute server2 code: 


./client <loadbalancer host name> <port No of loadbalancer in step 5>

ex : 
./client localhost 8001
