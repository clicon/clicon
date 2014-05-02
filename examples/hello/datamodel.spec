# Example specification
name="datamodel"; # Name of syntax-tree
hello("Hello"){
    vector[nr]("hello vector"){
	<nr:int>("Number in vector");
	<greeting:rest default:"hello">("Greeting string");
    } 
    <status:bool>("Set a boolean status: off or on");
}
