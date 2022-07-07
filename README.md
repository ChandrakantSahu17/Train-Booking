How to Run:
 For Server
 1. $ gcc server.c -o server
 2. ./server
 For Client
 1. $ gcc client.c -o client
 2. ./client
 3. Enter username: admin
 4  Enter password: 12345
Once you login as admin you can add any number of user/agent/admin accounts and login using them.

Features of the Project
 1. On server initiation it will create 4 files - login.dat, booking.dat, bid.dat, train.dat
 2. When login as admin, you can do CRUD operations on user i.e. login.dat file and on Train i.e. train.dat
 3. You can not delete a user or train when there's a booking associated to user or train.
 4. An account as user with same username cannot login again if the session is on from different terminal.
 5. An account as agent with same username can login again from different terminal.
 6. Any client logged-in cannot quit until and unless it has properly logged out.
 7. As user and agent you can do booking of the ticket, modify a ticket, cancel a ticket and view previous bookings.
 8. When you cancel the ticket the number of booked seats are reduced from the booked seats of the given train.
 9. When you modify the ticket i.e. the number of booked seats are reduced or increased from the booking, the booked seats of the given train are respectively reflected.
10. READ and WRITE Locks are applied throughout server side to ensure smooth booking and other operations.
