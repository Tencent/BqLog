package bq.test;

public abstract class test_base {
	private String name_;
	
	public test_base(String name) {
		this.name_ = name;
	}
	
	public String get_name() {
		return name_;
	}
	
	public abstract test_result test();
}
