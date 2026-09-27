class Zombie : MonoBehaviour
{
	Entity@ player;

	AnimatorComponent@ animator;
	private TransformComponent@ transform;
	private MovementComponent@ motor;

	void start()
    {
        print("Zombie started on entity: " + GetOwner().GetName());

        //@animator = GetOwner().GetAnimator();
        @transform = GetOwner().GetTransform();
        @motor = GetOwner().GetMovement();

        if (animator != null)
        {
        	animator.clearAllNotifies();

        	animator.addNotify("walk", 1.0f/24.0f, OnAnimNotify(this.doStep));
        	animator.addNotify("walk", 10.0f/24.0f, OnAnimNotify(this.doStep));
        	animator.addNotify("walk", 20.0f/24.0f, OnAnimNotify(this.doStep));
        	animator.addNotify("walk", 30.0f/24.0f, OnAnimNotify(this.doStep));
        	animator.addNotify("walk", 40.0f/24.0f, OnAnimNotify(this.doStep));
        	animator.addNotify("walk", 50.0f/24.0f, OnAnimNotify(this.doStep));

        	animator.addNotify("attack1", 10.0f/24.0f, OnAnimNotify(this.doAttack));
        	animator.addNotify("attack1", 19.0f/24.0f, OnAnimNotify(this.doAttack));

        	animator.Play("idle1", false);
        }
    }

    void update(float dt)
    {
    	if (player != null)
    	{
    		Vector3 playerPos = player.GetTransform().GetWorldPosition();
    		Vector3 myPos = transform.GetWorldPosition();

    		Vector3 dir = myPos - playerPos;

    		float angle = Math::atan2(dir.x, dir.z) * 180.0f / 3.14f;
    		Vector3 euler = Vector3(0, angle, 0);
    		transform.SetRotation(euler);

    		float dist = length(dir);

    		if (dist > 80.0f)
    		{
    			//Vector3 forward = transform.GetForward();
        		//Vector3 direction = forward * 64.0f;
        		//Vector3 newPosition = myPos + direction * dt;
        		//transform.SetPosition(newPosition);
        		motor.AddMovementInput(transform.GetForward(), 1.0f);

        		animator.Play("walk", false);
    		}
    		else
    		{
    			animator.Play("attack1", false);
    		}
    	}
    }

    private void doStep()
    {
    	PlaySound("assets/sounds/npc_step1.wav");
    }

    private void doAttack()
    {
    	PlaySound("assets/sounds/claw_strike1.wav");
    }
}